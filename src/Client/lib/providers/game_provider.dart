import 'package:flutter/foundation.dart';
import '../services/api_service.dart';

class GameProvider with ChangeNotifier {
  final ApiService _apiService = ApiService();

  List<Map<String, dynamic>> _decks = [];
  String? _currentSessionId;
  Map<String, dynamic>? _battleState;
  bool _isLoading = false;
  String? _error;
  String? _token;
  String? _userId;

  // Getters
  List<Map<String, dynamic>> get decks => _decks;
  String? get currentSessionId => _currentSessionId;
  Map<String, dynamic>? get battleState => _battleState;
  bool get isLoading => _isLoading;
  String? get error => _error;
  bool get isInBattle => _battleState != null;

  // Set auth info
  void setAuthInfo(String token, String userId) {
    _token = token;
    _userId = userId;
  }

  // Clear error
  void clearError() {
    _error = null;
    notifyListeners();
  }

  // Load user decks
  Future<bool> loadDecks() async {
    if (_token == null) return false;

    _isLoading = true;
    _error = null;
    notifyListeners();

    try {
      final response = await _apiService.getDecks(_token!);
      _decks = List<Map<String, dynamic>>.from(response['decks'] ?? []);
      _isLoading = false;
      notifyListeners();
      return true;
    } catch (e) {
      _error = e.toString();
      _isLoading = false;
      notifyListeners();
      return false;
    }
  }

  // Create deck
  Future<bool> createDeck(String name, List<String> cardIds) async {
    if (_token == null) return false;

    _isLoading = true;
    _error = null;
    notifyListeners();

    try {
      await _apiService.createDeck(_token!, name, cardIds);
      await loadDecks(); // Reload decks
      return true;
    } catch (e) {
      _error = e.toString();
      _isLoading = false;
      notifyListeners();
      return false;
    }
  }

  // Update deck
  Future<bool> updateDeck(
    String deckId,
    String name,
    List<String> cardIds,
  ) async {
    if (_token == null) return false;

    _isLoading = true;
    _error = null;
    notifyListeners();

    try {
      await _apiService.updateDeck(_token!, deckId, name, cardIds);
      await loadDecks(); // Reload decks
      return true;
    } catch (e) {
      _error = e.toString();
      _isLoading = false;
      notifyListeners();
      return false;
    }
  }

  // Delete deck
  Future<bool> deleteDeck(String deckId) async {
    if (_token == null) return false;

    _isLoading = true;
    _error = null;
    notifyListeners();

    try {
      await _apiService.deleteDeck(_token!, deckId);
      await loadDecks(); // Reload decks
      return true;
    } catch (e) {
      _error = e.toString();
      _isLoading = false;
      notifyListeners();
      return false;
    }
  }

  // Create game session
  Future<bool> createSession() async {
    if (_token == null) return false;

    try {
      final response = await _apiService.createSession(_token!);

      if (response['success'] == true) {
        _currentSessionId = response['session_id'];
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

  // Join game session
  Future<bool> joinSession(String sessionId) async {
    if (_token == null) return false;

    try {
      final response = await _apiService.joinSession(_token!, sessionId);

      if (response['success'] == true) {
        _currentSessionId = sessionId;

        // If the response includes a session status, update the battle state
        if (response['session_status'] != null) {
          _battleState = {
            'session_id': sessionId,
            'status': response['session_status'],
            // Add other session info as needed
          };
        }

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

  // Select deck for battle
  Future<bool> selectDeck(String sessionId, String deckId) async {
    if (_token == null) return false;

    try {
      final response = await _apiService.selectDeck(_token!, sessionId, deckId);

      if (response['success'] == true) {
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

  // Get waiting sessions
  Future<List<Map<String, dynamic>>> getSessions() async {
    if (_token == null) return [];

    try {
      final response = await _apiService.getSessions(_token!);

      if (response['success'] == true) {
        return List<Map<String, dynamic>>.from(response['sessions'] ?? []);
      }
      return [];
    } catch (e) {
      _error = e.toString();
      notifyListeners();
      return [];
    }
  }

  // Start battle
  Future<bool> startBattle(String sessionId, String deckId) async {
    if (_token == null) return false;

    _isLoading = true;
    _error = null;
    notifyListeners();

    try {
      final response = await _apiService.startBattle(
        _token!,
        sessionId,
        deckId,
      );

      if (response['success'] == true) {
        _battleState = response['battle'];
        _currentSessionId = sessionId;
        notifyListeners();
        return true;
      }
      return false;
    } catch (e) {
      _error = e.toString();
      _isLoading = false;
      notifyListeners();
      return false;
    }
  }

  // Refresh battle state
  Future<bool> refreshBattleState(String sessionId) async {
    if (_token == null) return false;

    try {
      final response = await _apiService.getBattleState(_token!, sessionId);

      if (response['success'] == true) {
        _battleState = response['battle'];
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

  // Play card
  Future<bool> playCard(String sessionId, int handIndex) async {
    if (_token == null || _userId == null) return false;

    try {
      final response = await _apiService.playCard(
        _token!,
        sessionId,
        _userId!,
        handIndex,
      );

      if (response['success'] == true) {
        _battleState = response['battle'];
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

  // Attack
  Future<bool> attack(
    String sessionId,
    int attackerIndex,
    int targetIndex,
  ) async {
    if (_token == null || _userId == null) return false;

    try {
      final response = await _apiService.attack(
        _token!,
        sessionId,
        _userId!,
        attackerIndex,
        targetIndex,
      );

      if (response['success'] == true) {
        _battleState = response['battle'];
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

  // End turn
  Future<bool> endTurn(String sessionId) async {
    if (_token == null || _userId == null) return false;

    try {
      final response = await _apiService.endTurn(_token!, sessionId, _userId!);

      if (response['success'] == true) {
        _battleState = response['battle'];
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

  // End session
  void endSession() {
    _currentSessionId = null;
    _battleState = null;
    _error = null;
    notifyListeners();
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
      }
      return false;
    } catch (e) {
      _error = e.toString();
      notifyListeners();
      return false;
    }
  }
}
