import 'package:flutter/foundation.dart';
import 'package:flutter/widgets.dart';
import 'dart:async';
import '../services/api_service.dart';
import '../services/websocket_service.dart';

class GameProvider with ChangeNotifier {
  final ApiService _apiService = ApiService();
  final WebSocketService _webSocketService = WebSocketService();

  List<Map<String, dynamic>> _sessions = [];
  String? _currentSessionId;
  Map<String, dynamic>? _battleState;
  bool _isLoading = false;
  String? _error;
  String? _token;
  bool _isConnectedToWebSocket = false;

  // Polling mechanism
  Timer? _sessionPollingTimer;
  bool _isPolling = false;
  static const Duration _pollingInterval = Duration(seconds: 3);

  // Getters
  List<Map<String, dynamic>> get sessions => _sessions;
  String? get currentSessionId => _currentSessionId;
  Map<String, dynamic>? get battleState => _battleState;
  bool get isLoading => _isLoading;
  String? get error => _error;
  bool get isInBattle => _battleState != null;
  bool get isConnectedToWebSocket => _isConnectedToWebSocket;
  WebSocketService get webSocketService => _webSocketService;
  bool get isPolling => _isPolling;

  // Set auth info
  void setAuthInfo(String token, String userId) {
    _token = token;
  }

  // Clear error
  void clearError() {
    _error = null;
    notifyListeners();
  }

  // Start polling for session updates
  void startSessionPolling() {
    if (_isPolling || _token == null) return;

    _isPolling = true;
    _sessionPollingTimer = Timer.periodic(_pollingInterval, (timer) {
      if (_token != null) {
        getSessions();
      }
    });
    notifyListeners();
    print('[GameProvider] Started session polling');
  }

  // Stop polling for session updates
  void stopSessionPolling() {
    _sessionPollingTimer?.cancel();
    _sessionPollingTimer = null;
    _isPolling = false;

    // Use post-frame callback to avoid calling notifyListeners during build
    WidgetsBinding.instance.addPostFrameCallback((_) {
      notifyListeners();
    });

    print('[GameProvider] Stopped session polling');
  }

  // Create game session
  Future<bool> createSession() async {
    if (_token == null) return false;

    _isLoading = true;
    _error = null;
    notifyListeners();

    try {
      final response = await _apiService.createSession(_token!);

      if (response['success'] == true) {
        _currentSessionId = response['session_id'];

        _isLoading = false;
        notifyListeners();
        return true;
      } else {
        _error = response['error'] ?? 'Failed to create session';
        _isLoading = false;
        notifyListeners();
        return false;
      }
    } catch (e) {
      _error = e.toString();
      _isLoading = false;
      notifyListeners();
      return false;
    }
  }

  // Join game session
  Future<bool> joinSession(String sessionId) async {
    if (_token == null) return false;

    _isLoading = true;
    _error = null;
    notifyListeners();

    try {
      print('[GameProvider] Joining session: $sessionId');
      final response = await _apiService.joinSession(_token!, sessionId);
      print('[GameProvider] Join session response: $response');

      if (response['success'] == true) {
        _currentSessionId = sessionId;

        // If the response includes a session status, update the battle state
        if (response['session_status'] != null) {
          _battleState = {
            'session_id': sessionId,
            'status': response['session_status'],
            'session_status': response['session_status'],
          };
        }

        _isLoading = false;
        notifyListeners();
        return true;
      } else {
        _error = response['error'] ?? 'Failed to join session';
        _isLoading = false;
        notifyListeners();
        return false;
      }
    } catch (e, stackTrace) {
      print('[GameProvider] Error joining session: $e');
      print('[GameProvider] Stack trace: $stackTrace');
      _error = e.toString();
      _isLoading = false;
      notifyListeners();
      return false;
    }
  }

  // Get waiting sessions
  Future<List<Map<String, dynamic>>> getSessions() async {
    if (_token == null) return [];

    try {
      print('[GameProvider] Fetching sessions...');
      final response = await _apiService.getSessions(_token!);

      if (response['success'] == true) {
        final newSessions = List<Map<String, dynamic>>.from(
          response['sessions'] ?? [],
        );
        print('[GameProvider] Received ${newSessions.length} sessions');

        // Only update if sessions actually changed
        bool sessionsChanged = _sessions.length != newSessions.length;
        if (!sessionsChanged) {
          for (int i = 0; i < _sessions.length; i++) {
            if (_sessions[i]['id'] != newSessions[i]['id'] ||
                _sessions[i]['status'] != newSessions[i]['status']) {
              sessionsChanged = true;
              break;
            }
          }
        }

        if (sessionsChanged) {
          _sessions = newSessions;
          print(
            '[GameProvider] Sessions updated: ${_sessions.length} sessions',
          );
          notifyListeners();
        }

        return _sessions;
      }
      return [];
    } catch (e, stackTrace) {
      print('[GameProvider] Error fetching sessions: $e');
      print('[GameProvider] Stack trace: $stackTrace');
      _error = e.toString();
      notifyListeners();
      return [];
    }
  }

  // Connect to WebSocket for battle
  Future<bool> connectToBattle(String sessionId, String userId) async {
    try {
      final connected = await _webSocketService.connectToBattle(
        sessionId,
        userId,
      );
      if (connected) {
        _isConnectedToWebSocket = true;
        _currentSessionId = sessionId;

        // Listen for WebSocket messages
        _webSocketService.messageStream?.listen((message) {
          _handleWebSocketMessage(message);
        });

        notifyListeners();
        return true;
      }
      return false;
    } catch (e) {
      _error = e.toString();
      notifyListeners();
      return false;
    }
  }

  // Handle WebSocket messages
  void _handleWebSocketMessage(Map<String, dynamic> message) {
    print('[GameProvider] Received WebSocket message: $message');

    // Check if this is an error message
    if (message['success'] == false) {
      _error = message['error'];
      notifyListeners();
      return;
    }

    // Check if this is a battle state message (has session_id and players)
    if (message['session_id'] != null && message['players'] != null) {
      _battleState = message;
      _error = null; // Clear error if we have a valid battle state
      notifyListeners();
      return;
    }

    // Handle other message types if needed
    final type = message['type'];
    if (type != null) {
      switch (type) {
        case 'battle_state':
          _battleState = message['battle_state'];
          notifyListeners();
          break;
        case 'game_start':
          _battleState = message['battle_state'];
          notifyListeners();
          break;
        case 'card_played':
          _battleState = message['battle_state'];
          notifyListeners();
          break;
        case 'attack':
          _battleState = message['battle_state'];
          notifyListeners();
          break;
        case 'turn_ended':
          _battleState = message['battle_state'];
          notifyListeners();
          break;
        case 'game_over':
          _battleState = message['battle_state'];
          notifyListeners();
          break;
        case 'error':
          _error = message['message'];
          notifyListeners();
          break;
        default:
          print('[GameProvider] Unknown message type: $type');
      }
    } else {
      print('[GameProvider] Message without type field: $message');
    }
  }

  // WebSocket actions
  void playCard(int handIndex) {
    if (_isConnectedToWebSocket) {
      _webSocketService.playCard(handIndex);
    }
  }

  void attack(int attackerIndex, int targetIndex) {
    if (_isConnectedToWebSocket) {
      _webSocketService.attack(attackerIndex, targetIndex);
    }
  }

  void endTurn() {
    if (_isConnectedToWebSocket) {
      _webSocketService.endTurn();
    }
  }

  void surrender() {
    if (_isConnectedToWebSocket) {
      _webSocketService.surrender();
    }
  }

  void sendReady() {
    if (_isConnectedToWebSocket) {
      _webSocketService.sendReady();
    }
  }

  // Leave session
  Future<bool> leaveSession(String sessionId) async {
    if (_token == null) return false;

    try {
      final response = await _apiService.leaveSession(_token!, sessionId);

      if (response['success'] == true) {
        // Clear session state
        endSession();
        return true;
      } else {
        _error = response['error'] ?? 'Failed to leave session';
        notifyListeners();
        return false;
      }
    } catch (e) {
      _error = e.toString();
      notifyListeners();
      return false;
    }
  }

  // End session
  void endSession() {
    _currentSessionId = null;
    _battleState = null;
    _error = null;
    _isConnectedToWebSocket = false;
    _webSocketService.disconnect();

    // Restart session polling when leaving battle
    if (_token != null) {
      startSessionPolling();
    }

    notifyListeners();
  }

  // Dispose
  @override
  void dispose() {
    stopSessionPolling();
    _webSocketService.dispose();
    super.dispose();
  }
}
