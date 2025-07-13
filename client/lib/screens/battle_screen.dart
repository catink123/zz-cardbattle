import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/game_provider.dart';
import '../providers/auth_provider.dart';
import '../main.dart';

// Card size constants
const double kCardWidth = 80;
const double kCardHeight = 100;
const double kFieldHeight = 140;

class BattleScreen extends StatefulWidget {
  final String sessionId;

  const BattleScreen({super.key, required this.sessionId});

  @override
  State<BattleScreen> createState() => _BattleScreenState();
}

class _BattleScreenState extends State<BattleScreen>
    with TickerProviderStateMixin {
  bool _isLoading = true;
  bool _isConnected = false;
  String? _error;
  Map<String, dynamic>? _battleState;
  String? _currentUserId;

  // Animation controllers
  late AnimationController _fadeController;
  late AnimationController _slideController;
  late AnimationController _cardController;

  // Selected card for attack
  int? _selectedCardIndex;
  bool _isAttacking = false;

  @override
  void initState() {
    super.initState();
    _initializeAnimations();
    _initializeBattle();
  }

  void _initializeAnimations() {
    _fadeController = AnimationController(
      duration: const Duration(milliseconds: 500),
      vsync: this,
    );
    _slideController = AnimationController(
      duration: const Duration(milliseconds: 300),
      vsync: this,
    );
    _cardController = AnimationController(
      duration: const Duration(milliseconds: 200),
      vsync: this,
    );
  }

  @override
  void dispose() {
    _fadeController.dispose();
    _slideController.dispose();
    _cardController.dispose();
    super.dispose();
  }

  Future<void> _initializeBattle() async {
    try {
      setState(() {
        _isLoading = true;
        _error = null;
      });

      final authProvider = Provider.of<AuthProvider>(context, listen: false);
      final gameProvider = Provider.of<GameProvider>(context, listen: false);

      _currentUserId = authProvider.userId;

      print(
        '[BattleScreen] Initializing battle for session: ${widget.sessionId}',
      );
      print('[BattleScreen] Current user ID: $_currentUserId');

      final connected = await gameProvider.connectToBattle(
        widget.sessionId,
        _currentUserId!,
      );

      print('[BattleScreen] WebSocket connection result: $connected');

      if (connected) {
        print('[BattleScreen] WebSocket connected, updating state');
        setState(() {
          _isConnected = true;
          _isLoading = false;
        });

        // Start animations
        _fadeController.forward();
        _slideController.forward();

        // Send ready signal
        print('[BattleScreen] Sending ready signal');
        gameProvider.sendReady();
      } else {
        print('[BattleScreen] WebSocket connection failed');
        setState(() {
          _error = 'Failed to connect to battle';
          _isLoading = false;
        });
      }
    } catch (e, stackTrace) {
      print('[BattleScreen] Exception during initialization: $e');
      print('[BattleScreen] Stack trace: $stackTrace');
      setState(() {
        _error = 'Error initializing battle: $e';
        _isLoading = false;
      });
    }
  }

  Future<void> _surrender() async {
    try {
      final gameProvider = Provider.of<GameProvider>(context, listen: false);

      // Send surrender action
      gameProvider.surrender();

      // Wait a moment for the server to process
      await Future.delayed(const Duration(milliseconds: 500));

      // Clean up and leave
      await _leaveSession();
    } catch (e) {
      print('[BattleScreen] Error surrendering: $e');
      await _leaveSession();
    }
  }

  Future<void> _leaveSession() async {
    try {
      final gameProvider = Provider.of<GameProvider>(context, listen: false);

      // Clean up WebSocket connection
      gameProvider.webSocketService.disconnect();

      // Leave the session
      await gameProvider.leaveSession(widget.sessionId);

      // Restart session polling
      gameProvider.startSessionPolling();

      // Navigate back using global navigator key
      CardBattleApp.navigatorKey.currentState?.pop();
    } catch (e) {
      print('[BattleScreen] Error leaving session: $e');
      // Still try to navigate back even if there's an error
      CardBattleApp.navigatorKey.currentState?.pop();
    }
  }

  void _playCard(int handIndex) {
    final gameProvider = Provider.of<GameProvider>(context, listen: false);
    gameProvider.playCard(handIndex);

    // Animate card play
    _cardController.forward().then((_) => _cardController.reverse());
  }

  void _selectCardForAttack(int cardIndex) {
    setState(() {
      _selectedCardIndex = cardIndex;
      _isAttacking = true;
    });
  }

  void _attackTarget(int targetIndex) {
    if (_selectedCardIndex != null) {
      final gameProvider = Provider.of<GameProvider>(context, listen: false);
      gameProvider.attack(_selectedCardIndex!, targetIndex);

      setState(() {
        _selectedCardIndex = null;
        _isAttacking = false;
      });
    }
  }

  void _endTurn() {
    final gameProvider = Provider.of<GameProvider>(context, listen: false);
    gameProvider.endTurn();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xFF1a1a2e),
      appBar: AppBar(
        title: Text('Battle', style: TextStyle(color: Colors.white)),
        backgroundColor: const Color(0xFF16213e),
        foregroundColor: Colors.white,
        elevation: 0,
        actions: [
          IconButton(
            onPressed: _surrender,
            icon: const Icon(Icons.flag),
            tooltip: 'Surrender',
          ),
        ],
      ),
      body: Consumer<GameProvider>(
        builder: (context, game, child) {
          // Update local state from game provider
          if (game.battleState != null && game.battleState != _battleState) {
            _battleState = game.battleState;
            _error = null; // Clear error if we have a valid battle state
          }
          if (game.error != null && game.error != _error) {
            _error = game.error;
          }
          if (game.isConnectedToWebSocket != _isConnected) {
            _isConnected = game.isConnectedToWebSocket;
          }

          if (_isLoading) {
            return _buildLoadingScreen();
          }

          if (_error != null) {
            WidgetsBinding.instance.addPostFrameCallback((_) {
              final scaffold = ScaffoldMessenger.of(context);
              scaffold.clearSnackBars();
              scaffold.showSnackBar(
                SnackBar(
                  content: Text(_error!),
                  backgroundColor: Colors.red,
                  duration: const Duration(seconds: 3),
                ),
              );
            });
          }

          if (!_isConnected) {
            return _buildDisconnectedScreen();
          }

          // Battle UI
          return _buildBattleUI();
        },
      ),
    );
  }

  Widget _buildLoadingScreen() {
    return FadeTransition(
      opacity: _fadeController,
      child: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            const CircularProgressIndicator(
              valueColor: AlwaysStoppedAnimation<Color>(Colors.blue),
            ),
            const SizedBox(height: 16),
            const Text(
              'Connecting to battle...',
              style: TextStyle(color: Colors.white, fontSize: 18),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildDisconnectedScreen() {
    return Center(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          const Icon(Icons.wifi_off, size: 64, color: Colors.orange),
          const SizedBox(height: 16),
          const Text(
            'Not connected to battle',
            style: TextStyle(color: Colors.white, fontSize: 18),
          ),
        ],
      ),
    );
  }

  Widget _buildBattleUI() {
    if (_battleState == null) {
      return Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            const Icon(Icons.hourglass_empty, size: 64, color: Colors.blue),
            const SizedBox(height: 16),
            const Text(
              'Waiting for battle to start...',
              style: TextStyle(color: Colors.white, fontSize: 18),
            ),
          ],
        ),
      );
    }

    final players = _battleState!['players'] as Map<String, dynamic>?;
    final currentTurn = _battleState!['current_turn'] as String?;
    final turnNumber = _battleState!['turn_number'] as int?;
    final isFinished = _battleState!['is_finished'] as bool? ?? false;
    final winner = _battleState!['winner'] as String?;

    if (isFinished) {
      return _buildGameOverScreen(winner);
    }

    // Get my player for hand and turn
    final myPlayer = players?[_currentUserId] as Map<String, dynamic>?;
    final isMyTurn = currentTurn == _currentUserId;
    final handRaw = myPlayer != null ? myPlayer['hand'] : null;
    final hand = (handRaw is List) ? handRaw : <dynamic>[];

    return Column(
      children: [
        // Main battle area (scrollable if needed)
        Expanded(
          child: SingleChildScrollView(
            child: Column(
              children: [
                _buildBattleStatusBar(currentTurn, turnNumber),
                _buildGameArea(players, currentTurn),
              ],
            ),
          ),
        ),
        // Hand panel (always at bottom)
        SizedBox(
          height: kCardHeight + 20,
          width: double.infinity,
          child: _buildHand(hand, isMyTurn),
        ),
        // End Turn button (always at bottom)
        if (isMyTurn)
          Padding(
            padding: const EdgeInsets.only(bottom: 16, top: 8),
            child: _buildActionButtons(),
          ),
      ],
    );
  }

  Widget _buildBattleStatusBar(String? currentTurn, int? turnNumber) {
    final isMyTurn = currentTurn == _currentUserId;

    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: const Color(0xFF0f3460),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withOpacity(0.2),
            blurRadius: 4,
            offset: const Offset(0, 2),
          ),
        ],
      ),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text(
            'Turn: ${turnNumber ?? 0}',
            style: const TextStyle(
              color: Colors.white,
              fontWeight: FontWeight.bold,
            ),
          ),
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
            decoration: BoxDecoration(
              color: isMyTurn ? Colors.green : Colors.orange,
              borderRadius: BorderRadius.circular(20),
            ),
            child: Text(
              isMyTurn ? 'Your Turn' : 'Opponent\'s Turn',
              style: const TextStyle(
                color: Colors.white,
                fontWeight: FontWeight.bold,
                fontSize: 12,
              ),
            ),
          ),
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
            decoration: BoxDecoration(
              color: _isConnected ? Colors.green : Colors.red,
              borderRadius: BorderRadius.circular(12),
            ),
            child: Text(
              _isConnected ? 'Connected' : 'Disconnected',
              style: const TextStyle(color: Colors.white, fontSize: 10),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildGameArea(Map<String, dynamic>? players, String? currentTurn) {
    if (players == null) return const SizedBox();

    // Find player data
    final myPlayer = _getMyPlayer(players);
    final opponentPlayer = _getOpponentPlayer(players);
    final isMyTurn = currentTurn == _currentUserId;

    return Column(
      children: [
        // Opponent side (top)
        _buildOpponentSide(opponentPlayer),

        // Center divider
        Container(
          height: 2,
          color: Colors.grey[600],
          margin: const EdgeInsets.symmetric(vertical: 8),
        ),

        // My side (bottom)
        _buildMySide(myPlayer, isMyTurn),
      ],
    );
  }

  Map<String, dynamic>? _getMyPlayer(Map<String, dynamic> players) {
    return players[_currentUserId] as Map<String, dynamic>?;
  }

  Map<String, dynamic>? _getOpponentPlayer(Map<String, dynamic> players) {
    for (String key in players.keys) {
      if (key != _currentUserId) {
        return players[key] as Map<String, dynamic>?;
      }
    }
    return null;
  }

  Widget _buildOpponentSide(Map<String, dynamic>? player) {
    if (player == null) return const SizedBox();

    final health = player['health'] as int? ?? 0;
    final maxHealth = player['max_health'] as int? ?? 0;
    final mana = player['mana'] as int? ?? 0;
    final maxMana = player['max_mana'] as int? ?? 0;
    final fieldRaw = player['field'];
    final field = (fieldRaw is List) ? fieldRaw : <dynamic>[];
    final deckCount =
        (player['deck'] as Map<String, dynamic>?)?['count'] as int? ?? 0;

    // If a card is selected, allow tapping the opponent's face to attack directly
    return Container(
      padding: const EdgeInsets.all(8),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          GestureDetector(
            onTap: (_selectedCardIndex != null && _isMyTurn())
                ? () => _attackTarget(-1)
                : null,
            child: Stack(
              alignment: Alignment.center,
              children: [
                _buildPlayerStats(
                  'Opponent',
                  health,
                  maxHealth,
                  mana,
                  maxMana,
                  deckCount,
                  isOpponent: true,
                ),
                if (_selectedCardIndex != null && _isMyTurn())
                  Positioned(
                    right: 8,
                    top: 8,
                    child: Container(
                      padding: const EdgeInsets.symmetric(
                        horizontal: 8,
                        vertical: 4,
                      ),
                      decoration: BoxDecoration(
                        color: Colors.red.withOpacity(0.8),
                        borderRadius: BorderRadius.circular(8),
                      ),
                      child: const Text(
                        'Attack Face',
                        style: TextStyle(
                          color: Colors.white,
                          fontWeight: FontWeight.bold,
                          fontSize: 10,
                        ),
                      ),
                    ),
                  ),
              ],
            ),
          ),
          const SizedBox(height: 8),
          // Opponent field (hidden cards)
          SizedBox(
            height: kCardHeight + 20,
            child: _buildField(field, isOpponent: true, isMyTurn: _isMyTurn()),
          ),
        ],
      ),
    );
  }

  Widget _buildMySide(Map<String, dynamic>? player, bool isMyTurn) {
    if (player == null) return const SizedBox();

    final health = player['health'] as int? ?? 0;
    final maxHealth = player['max_health'] as int? ?? 0;
    final mana = player['mana'] as int? ?? 0;
    final maxMana = player['max_mana'] as int? ?? 0;
    final fieldRaw = player['field'];
    final field = (fieldRaw is List) ? fieldRaw : <dynamic>[];
    final deckCount =
        (player['deck'] as Map<String, dynamic>?)?['count'] as int? ?? 0;

    return Container(
      padding: const EdgeInsets.all(8),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          _buildPlayerStats(
            'You',
            health,
            maxHealth,
            mana,
            maxMana,
            deckCount,
            isOpponent: false,
          ),
          const SizedBox(height: 8),
          // My field
          SizedBox(
            height: kCardHeight + 20,
            child: Stack(
              children: [
                _buildField(field, isOpponent: false, isMyTurn: isMyTurn),
                if (_selectedCardIndex != null && isMyTurn)
                  Positioned(
                    left: 0,
                    right: 0,
                    top: 0,
                    child: Center(
                      child: Container(
                        padding: const EdgeInsets.symmetric(
                          horizontal: 12,
                          vertical: 6,
                        ),
                        decoration: BoxDecoration(
                          color: Colors.yellow.withOpacity(0.9),
                          borderRadius: BorderRadius.circular(8),
                        ),
                        child: const Text(
                          'Select a target to attack',
                          style: TextStyle(
                            color: Colors.black,
                            fontWeight: FontWeight.bold,
                            fontSize: 12,
                          ),
                        ),
                      ),
                    ),
                  ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildPlayerStats(
    String playerName,
    int health,
    int maxHealth,
    int mana,
    int maxMana,
    int deckCount, {
    required bool isOpponent,
  }) {
    return Container(
      margin: const EdgeInsets.symmetric(vertical: 8, horizontal: 4),
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
      decoration: BoxDecoration(
        color: const Color(0xFF0f3460),
        borderRadius: BorderRadius.circular(12),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withOpacity(0.2),
            blurRadius: 4,
            offset: const Offset(0, 2),
          ),
        ],
      ),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          // Player name
          Expanded(
            child: Text(
              playerName,
              style: const TextStyle(
                color: Colors.white,
                fontWeight: FontWeight.bold,
                fontSize: 16,
              ),
              textAlign: TextAlign.center,
            ),
          ),
          const SizedBox(width: 16),
          // Health
          Row(
            children: [
              const Icon(Icons.favorite, color: Colors.red, size: 20),
              const SizedBox(width: 4),
              Text(
                '$health/$maxHealth',
                style: const TextStyle(
                  color: Colors.white,
                  fontWeight: FontWeight.bold,
                ),
              ),
            ],
          ),
          const SizedBox(width: 16),
          // Mana
          Row(
            children: [
              const Icon(Icons.water_drop, color: Colors.blue, size: 20),
              const SizedBox(width: 4),
              Text(
                '$mana/$maxMana',
                style: const TextStyle(
                  color: Colors.white,
                  fontWeight: FontWeight.bold,
                ),
              ),
            ],
          ),
          const SizedBox(width: 16),
          // Deck
          Row(
            children: [
              const Icon(Icons.style, color: Colors.grey, size: 20),
              const SizedBox(width: 4),
              Text(
                '$deckCount',
                style: const TextStyle(
                  color: Colors.white,
                  fontWeight: FontWeight.bold,
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }

  Widget _buildField(
    List<dynamic> field, {
    required bool isOpponent,
    bool isMyTurn = false,
  }) {
    return Container(
      decoration: BoxDecoration(
        color: const Color(0xFF533483).withOpacity(0.3),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: Colors.grey[600]!),
      ),
      width: double.infinity,
      child: field.isEmpty
          ? const Center(
              child: Text(
                'No cards in play',
                style: TextStyle(color: Colors.white70),
              ),
            )
          : LayoutBuilder(
              builder: (context, constraints) {
                return Center(
                  child: SizedBox(
                    height: kCardHeight + 20,
                    child: ListView.separated(
                      scrollDirection: Axis.horizontal,
                      padding: const EdgeInsets.symmetric(horizontal: 8),
                      itemCount: field.length,
                      separatorBuilder: (context, index) =>
                          const SizedBox(width: 8),
                      itemBuilder: (context, index) {
                        final card = field[index] as Map<String, dynamic>;
                        Widget cardWidget;
                        if (isOpponent &&
                            _selectedCardIndex != null &&
                            isMyTurn) {
                          cardWidget = GestureDetector(
                            onTap: () => _attackTarget(index),
                            child: _buildCardWidget(
                              card,
                              isPlayable: false,
                              isSelected: false,
                              width: kCardWidth,
                              height: kCardHeight,
                              highlight: true,
                            ),
                          );
                        } else if (!isOpponent && isMyTurn) {
                          cardWidget = _buildCardWidget(
                            card,
                            isPlayable: true,
                            isSelected: _selectedCardIndex == index,
                            onTap: () => _selectCardForAttack(index),
                            width: kCardWidth,
                            height: kCardHeight,
                          );
                        } else {
                          cardWidget = _buildCardWidget(
                            card,
                            isPlayable: false,
                            isSelected: false,
                            width: kCardWidth,
                            height: kCardHeight,
                          );
                        }
                        return Center(child: cardWidget);
                      },
                    ),
                  ),
                );
              },
            ),
    );
  }

  Widget _buildHand(List<dynamic> hand, bool isMyTurn) {
    return Container(
      decoration: BoxDecoration(
        color: const Color(0xFF533483).withOpacity(0.3),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: Colors.grey[600]!),
      ),
      width: double.infinity,
      child: hand.isEmpty
          ? const Center(
              child: Text(
                'No cards in hand',
                style: TextStyle(color: Colors.white70),
              ),
            )
          : LayoutBuilder(
              builder: (context, constraints) {
                return Center(
                  child: SizedBox(
                    height: kCardHeight + 20,
                    child: ListView.separated(
                      scrollDirection: Axis.horizontal,
                      padding: const EdgeInsets.symmetric(horizontal: 8),
                      itemCount: hand.length,
                      separatorBuilder: (context, index) =>
                          const SizedBox(width: 12),
                      itemBuilder: (context, index) {
                        final card = hand[index] as Map<String, dynamic>;
                        return Center(
                          child: _buildCardWidget(
                            card,
                            isPlayable: isMyTurn,
                            onTap: isMyTurn ? () => _playCard(index) : null,
                            width: kCardWidth,
                            height: kCardHeight,
                          ),
                        );
                      },
                    ),
                  ),
                );
              },
            ),
    );
  }

  Widget _buildCardWidget(
    Map<String, dynamic> card, {
    required bool isPlayable,
    bool isSelected = false,
    VoidCallback? onTap,
    double width = kCardWidth,
    double height = kCardHeight,
    bool highlight = false,
  }) {
    final name = card['name'] as String? ?? 'Unknown';
    final attack = card['attack'] as int? ?? 0;
    final defense = card['defense'] as int? ?? 0;
    final manaCost = card['mana_cost'] as int? ?? 0;

    return GestureDetector(
      onTap: onTap,
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 200),
        width: width,
        height: height,
        margin: const EdgeInsets.symmetric(vertical: 4),
        decoration: BoxDecoration(
          color: isSelected
              ? Colors.yellow.withOpacity(0.3)
              : highlight
              ? Colors.red.withOpacity(0.2)
              : isPlayable
              ? const Color(0xFF4CAF50).withOpacity(0.3)
              : const Color(0xFF666666).withOpacity(0.3),
          borderRadius: BorderRadius.circular(10),
          border: Border.all(
            color: isSelected
                ? Colors.yellow
                : highlight
                ? Colors.red
                : isPlayable
                ? Colors.green
                : Colors.grey,
            width: isSelected ? 3 : 2,
          ),
          boxShadow: isSelected
              ? [
                  BoxShadow(
                    color: Colors.yellow.withOpacity(0.5),
                    blurRadius: 8,
                    spreadRadius: 2,
                  ),
                ]
              : null,
        ),
        child: Padding(
          padding: const EdgeInsets.all(6),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              // Mana cost
              Align(
                alignment: Alignment.topRight,
                child: Container(
                  padding: const EdgeInsets.all(4),
                  decoration: const BoxDecoration(
                    color: Colors.blue,
                    shape: BoxShape.circle,
                  ),
                  child: Text(
                    '$manaCost',
                    style: const TextStyle(
                      color: Colors.white,
                      fontSize: 12,
                      fontWeight: FontWeight.bold,
                    ),
                  ),
                ),
              ),
              const SizedBox(height: 4),
              // Card name
              Expanded(
                child: Center(
                  child: Text(
                    name,
                    style: const TextStyle(
                      color: Colors.white,
                      fontSize: 13,
                      fontWeight: FontWeight.bold,
                    ),
                    textAlign: TextAlign.center,
                    maxLines: 2,
                    overflow: TextOverflow.ellipsis,
                  ),
                ),
              ),
              const SizedBox(height: 4),
              // Attack/Defense
              Row(
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                children: [
                  Row(
                    children: [
                      const Icon(Icons.flash_on, color: Colors.red, size: 14),
                      const SizedBox(width: 2),
                      Text(
                        '$attack',
                        style: const TextStyle(
                          fontSize: 12,
                          color: Colors.red,
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                    ],
                  ),
                  Row(
                    children: [
                      const Icon(Icons.shield, color: Colors.green, size: 14),
                      const SizedBox(width: 2),
                      Text(
                        '$defense',
                        style: const TextStyle(
                          fontSize: 12,
                          color: Colors.green,
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                    ],
                  ),
                ],
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildActionButtons() {
    return Container(
      padding: const EdgeInsets.all(16),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceEvenly,
        children: [
          ElevatedButton.icon(
            onPressed: _endTurn,
            icon: const Icon(Icons.skip_next),
            label: const Text('End Turn'),
            style: ElevatedButton.styleFrom(
              backgroundColor: Colors.orange,
              foregroundColor: Colors.white,
              padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildGameOverScreen(String? winner) {
    final isWinner = winner == _currentUserId;

    return FadeTransition(
      opacity: _fadeController,
      child: Center(
        child: Container(
          padding: const EdgeInsets.all(32),
          decoration: BoxDecoration(
            color: const Color(0xFF0f3460),
            borderRadius: BorderRadius.circular(20),
            boxShadow: [
              BoxShadow(
                color: Colors.black.withOpacity(0.3),
                blurRadius: 10,
                offset: const Offset(0, 5),
              ),
            ],
          ),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              Icon(
                isWinner ? Icons.emoji_events : Icons.sentiment_dissatisfied,
                size: 80,
                color: isWinner ? Colors.yellow : Colors.grey,
              ),
              const SizedBox(height: 24),
              Text(
                isWinner ? 'Victory!' : 'Defeat',
                style: TextStyle(
                  fontSize: 32,
                  fontWeight: FontWeight.bold,
                  color: isWinner ? Colors.green : Colors.red,
                ),
              ),
              const SizedBox(height: 16),
              Text(
                isWinner ? 'You won the battle!' : 'Better luck next time!',
                style: const TextStyle(color: Colors.white, fontSize: 18),
                textAlign: TextAlign.center,
              ),
              const SizedBox(height: 32),
              ElevatedButton(
                onPressed: _leaveSession,
                style: ElevatedButton.styleFrom(
                  backgroundColor: Colors.blue,
                  foregroundColor: Colors.white,
                  padding: const EdgeInsets.symmetric(
                    horizontal: 32,
                    vertical: 16,
                  ),
                ),
                child: const Text('Leave Battle'),
              ),
            ],
          ),
        ),
      ),
    );
  }

  // Helper to check if it's my turn
  bool _isMyTurn() {
    final currentTurn = _battleState?['current_turn'] as String?;
    return currentTurn == _currentUserId;
  }
}
