import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'dart:async';
import '../providers/game_provider.dart';
import '../providers/auth_provider.dart';
import '../services/api_service.dart';
import '../services/websocket_service.dart';

class BattleScreen extends StatefulWidget {
  final String sessionId;

  const BattleScreen({super.key, required this.sessionId});

  @override
  State<BattleScreen> createState() => _BattleScreenState();
}

class _BattleScreenState extends State<BattleScreen> {
  Timer? _pollTimer;
  bool _isLoading = true;
  Map<String, dynamic>? _battleState;
  String? _error;
  WebSocketService? _wsService;
  StreamSubscription<Map<String, dynamic>>? _wsSubscription;

  @override
  void initState() {
    super.initState();
    _initializeWebSocket();

    // Set up a timer to refresh battle state every 3 seconds
    _pollTimer = Timer.periodic(const Duration(seconds: 3), (timer) {
      if (!_isLoading && _error == null) {
        final authProvider = Provider.of<AuthProvider>(context, listen: false);
        final token = authProvider.token;
        if (token != null) {
          print('Auto-refreshing battle state...');
          if (_wsService?.isConnected == true) {
            _wsService!.requestBattleState();
          } else {
            _loadBattleState(token);
          }
        }
      }
    });

    // Eagerly load decks for deck selection
    final gameProvider = Provider.of<GameProvider>(context, listen: false);
    Future.microtask(() async {
      await gameProvider.loadDecks();
    });
  }

  @override
  void dispose() {
    print('BattleScreen: Disposing...');

    // Leave the session when disposing
    _leaveSession();

    // Cancel the poll timer first
    _pollTimer?.cancel();
    _pollTimer = null;

    // Cancel the WebSocket subscription
    _wsSubscription?.cancel();
    _wsSubscription = null;

    // Disconnect WebSocket properly
    if (_wsService != null) {
      print('BattleScreen: Disconnecting WebSocket...');
      _wsService!.disconnect();
      _wsService = null;
    }

    super.dispose();
    print('BattleScreen: Disposed successfully');
  }

  Future<void> _initializeWebSocket() async {
    final authProvider = Provider.of<AuthProvider>(context, listen: false);
    final currentUser = authProvider.currentUser;
    final token = authProvider.token;

    if (currentUser == null || token == null) {
      setState(() {
        _error = 'User not authenticated';
        _isLoading = false;
      });
      return;
    }

    print('Initializing WebSocket for session: ${widget.sessionId}');
    _wsService = WebSocketService();

    // Try to connect to WebSocket
    final connected = await _wsService!.connectToBattle(
      widget.sessionId,
      currentUser['id'],
    );

    print('WebSocket connection result: $connected');

    if (connected) {
      print('WebSocket connected successfully');
      // Listen for WebSocket messages
      _wsSubscription = _wsService!.messageStream?.listen(
        (message) {
          print('Received WebSocket message: $message');
          _handleWebSocketMessage(message);
        },
        onError: (error) {
          print('WebSocket error: $error');
          setState(() {
            _error = 'WebSocket error: $error';
            _isLoading = false;
          });
        },
        onDone: () {
          print('WebSocket stream closed');
        },
      );

      // Request initial battle state
      print('Requesting initial battle state');
      _wsService!.requestBattleState();
    } else {
      print('WebSocket connection failed, falling back to HTTP polling');
      // Fallback to HTTP polling
      _startPolling(token);
    }
  }

  void _handleWebSocketMessage(Map<String, dynamic> message) {
    print('Handling WebSocket message: $message');
    if (message['type'] == 'battle_state') {
      print('Received battle state: ${message['data']}');
      setState(() {
        _battleState = message['data'];
        _isLoading = false;
        _error = null;
      });
    } else if (message['type'] == 'session_update') {
      print('Received session update: $message');
      setState(() {
        _battleState = message;
        _isLoading = false;
        _error = null;
      });
    } else if (message['type'] == 'connection_established') {
      print('WebSocket connection established');
      _wsService!.requestBattleState();
    } else if (message['type'] == 'heartbeat') {
      print('Received heartbeat');
    } else if (message['type'] == 'error') {
      print('Received error message: ${message['message']}');
      setState(() {
        _error = message['message'];
        _isLoading = false;
      });
    } else {
      print('Unknown message type: ${message['type']}');
    }
  }

  void _startPolling(String token) {
    _pollTimer = Timer.periodic(const Duration(seconds: 2), (timer) {
      _loadBattleState(token);
    });
  }

  Future<void> _loadBattleState(String token) async {
    try {
      print('Loading battle state via HTTP...');
      final apiService = Provider.of<ApiService>(context, listen: false);
      final response = await apiService.getBattleState(token, widget.sessionId);
      print('HTTP battle state response: $response');

      if (mounted) {
        setState(() {
          _battleState = response;
          _isLoading = false;
          _error = null;
        });
        print('Updated battle state from HTTP: $_battleState');
      }
    } catch (e) {
      print('Error loading battle state: $e');
      if (mounted) {
        setState(() {
          _error = e.toString();
          _isLoading = false;
        });
      }
    }
  }

  Future<void> _playCard(int handIndex) async {
    final authProvider = Provider.of<AuthProvider>(context, listen: false);
    final token = authProvider.token;
    final userId = authProvider.userId;
    if (token == null || userId == null) return;
    if (_wsService?.isConnected == true) {
      _wsService!.playCard(handIndex);
    } else {
      // Fallback to HTTP
      try {
        final gameProvider = Provider.of<GameProvider>(context, listen: false);
        gameProvider.setAuthInfo(token, userId);
        await gameProvider.playCard(widget.sessionId, handIndex);
        _loadBattleState(token);
      } catch (e) {
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(SnackBar(content: Text('Error playing card: $e')));
      }
    }
  }

  Future<void> _attack(int attackerIndex, int targetIndex) async {
    final authProvider = Provider.of<AuthProvider>(context, listen: false);
    final token = authProvider.token;
    final userId = authProvider.userId;
    if (token == null || userId == null) return;
    if (_wsService?.isConnected == true) {
      _wsService!.attack(attackerIndex, targetIndex);
    } else {
      // Fallback to HTTP
      try {
        final gameProvider = Provider.of<GameProvider>(context, listen: false);
        gameProvider.setAuthInfo(token, userId);
        await gameProvider.attack(widget.sessionId, attackerIndex, targetIndex);
        _loadBattleState(token);
      } catch (e) {
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(SnackBar(content: Text('Error attacking: $e')));
      }
    }
  }

  Future<void> _endTurn() async {
    final authProvider = Provider.of<AuthProvider>(context, listen: false);
    final token = authProvider.token;
    final userId = authProvider.userId;
    if (token == null || userId == null) return;
    if (_wsService?.isConnected == true) {
      _wsService!.endTurn();
    } else {
      // Fallback to HTTP
      try {
        final gameProvider = Provider.of<GameProvider>(context, listen: false);
        gameProvider.setAuthInfo(token, userId);
        await gameProvider.endTurn(widget.sessionId);
        _loadBattleState(token);
      } catch (e) {
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(SnackBar(content: Text('Error ending turn: $e')));
      }
    }
  }

  Future<void> _leaveSession() async {
    try {
      final authProvider = Provider.of<AuthProvider>(context, listen: false);
      final gameProvider = Provider.of<GameProvider>(context, listen: false);

      if (authProvider.token != null && authProvider.userId != null) {
        print('BattleScreen: Leaving session ${widget.sessionId}');

        // Send leave message via WebSocket if connected
        if (_wsService?.isConnected == true) {
          _wsService!.sendMessage({
            'type': 'leave_session',
            'session_id': widget.sessionId,
            'user_id': authProvider.userId,
          });
        }

        // Also call API to leave session
        try {
          await gameProvider.leaveSession(widget.sessionId);
        } catch (e) {
          print('BattleScreen: Error leaving session via API: $e');
        }

        // Clear battle state
        gameProvider.endSession();
      }
    } catch (e) {
      print('BattleScreen: Error in _leaveSession: $e');
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Card Battle'),
        backgroundColor: Colors.deepPurple,
        foregroundColor: Colors.white,
        actions: [
          // WebSocket status indicator
          Container(
            margin: const EdgeInsets.all(8),
            width: 12,
            height: 12,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              color: _wsService?.isConnected == true
                  ? Colors.green
                  : Colors.red,
            ),
          ),
        ],
      ),
      body: _isLoading
          ? const Center(child: CircularProgressIndicator())
          : _error != null
          ? Center(
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  Text(
                    'Error: $_error',
                    style: const TextStyle(color: Colors.red),
                  ),
                  const SizedBox(height: 16),
                  ElevatedButton(
                    onPressed: _initializeWebSocket,
                    child: const Text('Retry'),
                  ),
                ],
              ),
            )
          : _buildBattleInterface(),
    );
  }

  Widget _buildBattleInterface() {
    if (_battleState == null)
      return const Center(child: Text('No battle data'));

    final authProvider = Provider.of<AuthProvider>(context, listen: false);
    final currentUserId = authProvider.currentUser?['id'];

    // Check if this is session data (waiting state) or battle data (active state)
    final sessionStatus = _battleState!['status'] as String?;
    final sessionId = _battleState!['session_id'] as String?;

    print('BattleScreen: Session status = $sessionStatus');
    print('BattleScreen: Session ID = $sessionId');
    print('BattleScreen: Battle state = $_battleState');

    if (sessionStatus == 'waiting') {
      // Show waiting screen
      return Column(
        children: [
          // Session info
          Container(
            padding: const EdgeInsets.all(16),
            color: Colors.grey[100],
            child: Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Text('Session: ${sessionId ?? 'Unknown'}'),
                Text('Status: Waiting for opponent'),
                Text(
                  'WS: ${_wsService?.isConnected == true ? 'Connected' : 'HTTP'}',
                ),
              ],
            ),
          ),

          // Waiting screen
          Expanded(
            child: Center(
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  const Icon(
                    Icons.hourglass_empty,
                    size: 64,
                    color: Colors.orange,
                  ),
                  const SizedBox(height: 20),
                  Text(
                    'Waiting for opponent to join...',
                    style: Theme.of(context).textTheme.headlineSmall,
                  ),
                  const SizedBox(height: 20),
                  Text(
                    'Session ID: ${sessionId ?? 'Unknown'}',
                    style: const TextStyle(fontSize: 16, color: Colors.grey),
                  ),
                  const SizedBox(height: 20),

                  // Session info
                  Container(
                    padding: const EdgeInsets.all(16),
                    decoration: BoxDecoration(
                      border: Border.all(color: Colors.grey[300]!),
                      borderRadius: BorderRadius.circular(8),
                    ),
                    child: Column(
                      children: [
                        Text('Host: ${_battleState!['host_id'] ?? 'Unknown'}'),
                        if (_battleState!['guest_id'] != null &&
                            _battleState!['guest_id'].toString().isNotEmpty)
                          Text('Guest: ${_battleState!['guest_id']}'),
                      ],
                    ),
                  ),

                  const SizedBox(height: 20),

                  // Action buttons
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                    children: [
                      ElevatedButton(
                        onPressed: () {
                          if (_wsService?.isConnected == true) {
                            _wsService!.requestBattleState();
                          } else {
                            _loadBattleState(authProvider.token!);
                          }
                        },
                        child: const Text('Refresh'),
                      ),
                    ],
                  ),
                ],
              ),
            ),
          ),
        ],
      );
    } else if (sessionStatus == 'ready_for_decks' || sessionStatus == 'ready') {
      // Show deck selection interface
      return _buildDeckSelectionInterface();
    } else {
      // Show battle interface (for active battles)
      // This will be implemented when battles are actually started
      return Column(
        children: [
          Container(
            padding: const EdgeInsets.all(16),
            color: Colors.grey[100],
            child: Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Text('Session: ${sessionId ?? 'Unknown'}'),
                Text('Status: $sessionStatus'),
                Text(
                  'WS: ${_wsService?.isConnected == true ? 'Connected' : 'HTTP'}',
                ),
              ],
            ),
          ),

          Expanded(
            child: Center(
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  const Icon(
                    Icons.sports_esports,
                    size: 64,
                    color: Colors.green,
                  ),
                  const SizedBox(height: 20),
                  Text(
                    'Battle Ready!',
                    style: Theme.of(context).textTheme.headlineSmall,
                  ),
                  const SizedBox(height: 20),
                  Text(
                    'Status: $sessionStatus',
                    style: const TextStyle(fontSize: 16),
                  ),
                  const SizedBox(height: 20),

                  ElevatedButton(
                    onPressed: () {
                      if (_wsService?.isConnected == true) {
                        _wsService!.requestBattleState();
                      } else {
                        _loadBattleState(authProvider.token!);
                      }
                    },
                    child: const Text('Refresh'),
                  ),
                ],
              ),
            ),
          ),
        ],
      );
    }
  }

  Widget _buildDeckSelectionInterface() {
    final authProvider = Provider.of<AuthProvider>(context, listen: false);
    final gameProvider = Provider.of<GameProvider>(context, listen: true);
    final currentUserId = authProvider.currentUser?['id'];
    final sessionId = _battleState!['session_id'] as String?;
    final decks = gameProvider.decks;
    return Column(
      children: [
        // Session info
        Container(
          padding: const EdgeInsets.all(16),
          color: Colors.grey[100],
          child: Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
              Text('Session: ${sessionId ?? 'Unknown'}'),
              Text('Status: Ready - Select Deck'),
              Text(
                'WS: ${_wsService?.isConnected == true ? 'Connected' : 'HTTP'}',
              ),
            ],
          ),
        ),
        Expanded(
          child: Center(
            child: decks.isEmpty
                ? Column(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      const Icon(Icons.warning, size: 64, color: Colors.orange),
                      const SizedBox(height: 20),
                      const Text('No decks available.'),
                      const SizedBox(height: 20),
                      ElevatedButton(
                        onPressed: () {
                          // TODO: Navigate to deck creation screen or show a dialog
                          ScaffoldMessenger.of(context).showSnackBar(
                            const SnackBar(
                              content: Text(
                                'Please create a deck from the home screen.',
                              ),
                            ),
                          );
                        },
                        child: const Text('Create Deck'),
                      ),
                    ],
                  )
                : Column(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      const Icon(Icons.deck, size: 64, color: Colors.blue),
                      const SizedBox(height: 20),
                      Text(
                        'Select Your Deck',
                        style: Theme.of(context).textTheme.headlineSmall,
                      ),
                      const SizedBox(height: 20),
                      Text(
                        'Both players must select decks to start the battle',
                        style: const TextStyle(
                          fontSize: 16,
                          color: Colors.grey,
                        ),
                      ),
                      const SizedBox(height: 20),
                      Expanded(
                        child: ListView.builder(
                          itemCount: decks.length,
                          itemBuilder: (context, index) {
                            final deck = decks[index];
                            return Card(
                              margin: const EdgeInsets.symmetric(
                                horizontal: 16,
                                vertical: 4,
                              ),
                              child: ListTile(
                                title: Text(deck['name'] ?? 'Unknown Deck'),
                                subtitle: Text(
                                  '${deck['card_ids']?.length ?? 0} cards',
                                ),
                                trailing: ElevatedButton(
                                  onPressed: () async {
                                    final success = await gameProvider
                                        .selectDeck(sessionId!, deck['id']);
                                    if (success) {
                                      ScaffoldMessenger.of(
                                        context,
                                      ).showSnackBar(
                                        const SnackBar(
                                          content: Text('Deck selected!'),
                                        ),
                                      );
                                      if (_wsService?.isConnected == true) {
                                        _wsService!.requestBattleState();
                                      } else {
                                        _loadBattleState(authProvider.token!);
                                      }
                                    } else {
                                      ScaffoldMessenger.of(
                                        context,
                                      ).showSnackBar(
                                        SnackBar(
                                          content: Text(
                                            'Error: ${gameProvider.error}',
                                          ),
                                        ),
                                      );
                                    }
                                  },
                                  child: const Text('Select'),
                                ),
                              ),
                            );
                          },
                        ),
                      ),
                      const SizedBox(height: 20),
                      Row(
                        mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                        children: [
                          ElevatedButton(
                            onPressed: () {
                              if (_wsService?.isConnected == true) {
                                _wsService!.requestBattleState();
                              } else {
                                _loadBattleState(authProvider.token!);
                              }
                            },
                            child: const Text('Refresh'),
                          ),
                          if (_battleState!['host_deck_id'] != null &&
                              _battleState!['host_deck_id']
                                  .toString()
                                  .isNotEmpty &&
                              _battleState!['guest_deck_id'] != null &&
                              _battleState!['guest_deck_id']
                                  .toString()
                                  .isNotEmpty)
                            ElevatedButton(
                              onPressed: () async {
                                try {
                                  final response = await gameProvider
                                      .startBattle(sessionId!, '');
                                  if (response) {
                                    ScaffoldMessenger.of(context).showSnackBar(
                                      const SnackBar(
                                        content: Text('Battle started!'),
                                      ),
                                    );
                                    if (_wsService?.isConnected == true) {
                                      _wsService!.requestBattleState();
                                    } else {
                                      _loadBattleState(authProvider.token!);
                                    }
                                  } else {
                                    ScaffoldMessenger.of(context).showSnackBar(
                                      SnackBar(
                                        content: Text(
                                          'Error: ${gameProvider.error}',
                                        ),
                                      ),
                                    );
                                  }
                                } catch (e) {
                                  ScaffoldMessenger.of(context).showSnackBar(
                                    SnackBar(
                                      content: Text(
                                        'Error starting battle: $e',
                                      ),
                                    ),
                                  );
                                }
                              },
                              style: ElevatedButton.styleFrom(
                                backgroundColor: Colors.green,
                                foregroundColor: Colors.white,
                              ),
                              child: const Text('Start Battle'),
                            ),
                        ],
                      ),
                    ],
                  ),
          ),
        ),
      ],
    );
  }
}
