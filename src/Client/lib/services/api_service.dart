import 'dart:convert';
import 'package:http/http.dart' as http;

class ApiService {
  static const String baseUrl = 'http://localhost:8080';

  // Auth endpoints
  static const String registerEndpoint = '/auth/register';
  static const String loginEndpoint = '/auth/login';
  static const String profileEndpoint = '/auth/profile';

  // Deck endpoints
  static const String createDeckEndpoint = '/deck/create';
  static const String getDecksEndpoint = '/deck/list';

  // Session endpoints
  static const String createSessionEndpoint = '/session/create';
  static const String joinSessionEndpoint = '/session/join';
  static const String getSessionsEndpoint = '/session/list';
  static const String startBattleEndpoint = '/battle/start';
  static const String getBattleStateEndpoint = '/battle/state';
  static const String playCardEndpoint = '/battle/play-card';
  static const String attackEndpoint = '/attack';
  static const String endTurnEndpoint = '/end-turn';

  // Health endpoint
  static const String healthEndpoint = '/health';

  Future<Map<String, dynamic>> makeRequest(
    String endpoint, {
    String method = 'GET',
    Map<String, dynamic>? body,
    Map<String, String>? headers,
    String? token,
  }) async {
    final uri = Uri.parse('$baseUrl$endpoint');
    final requestHeaders = {
      'Content-Type': 'application/json',
      if (token != null) 'Authorization': 'Bearer $token',
      ...?headers,
    };

    print('[ApiService] Making $method request to: $uri');
    print('[ApiService] Headers: $requestHeaders');
    if (body != null) {
      print('[ApiService] Body: $body');
    }

    http.Response response;

    try {
      switch (method.toUpperCase()) {
        case 'GET':
          response = await http.get(uri, headers: requestHeaders);
          break;
        case 'POST':
          response = await http.post(
            uri,
            headers: requestHeaders,
            body: body != null ? jsonEncode(body) : null,
          );
          break;
        case 'PUT':
          response = await http.put(
            uri,
            headers: requestHeaders,
            body: body != null ? jsonEncode(body) : null,
          );
          break;
        case 'DELETE':
          response = await http.delete(uri, headers: requestHeaders);
          break;
        default:
          throw Exception('Unsupported HTTP method: $method');
      }

      print('[ApiService] Response status: ${response.statusCode}');
      print('[ApiService] Response body: ${response.body}');

      if (response.statusCode >= 200 && response.statusCode < 300) {
        final decoded = jsonDecode(response.body);
        print('[ApiService] Decoded response: $decoded');
        return decoded;
      } else {
        print(
          '[ApiService] Error response: HTTP ${response.statusCode}: ${response.body}',
        );
        throw Exception('HTTP ${response.statusCode}: ${response.body}');
      }
    } catch (e) {
      print('[ApiService] Exception caught: $e');
      throw Exception('Network error: $e');
    }
  }

  // Auth methods
  Future<Map<String, dynamic>> register(
    String username,
    String email,
    String password,
  ) async {
    return await makeRequest(
      registerEndpoint,
      method: 'POST',
      body: {'username': username, 'email': email, 'password': password},
    );
  }

  Future<Map<String, dynamic>> login(String username, String password) async {
    return await makeRequest(
      loginEndpoint,
      method: 'POST',
      body: {'username': username, 'password': password},
    );
  }

  Future<Map<String, dynamic>> getProfile(String userId, String token) async {
    return await makeRequest('$profileEndpoint?user_id=$userId', token: token);
  }

  // Deck methods
  Future<Map<String, dynamic>> createDeck(
    String token,
    String name,
    List<String> cardIds,
  ) async {
    return await makeRequest(
      createDeckEndpoint,
      method: 'POST',
      token: token,
      body: {'name': name, 'card_ids': cardIds},
    );
  }

  Future<Map<String, dynamic>> getDecks(String token) async {
    return await makeRequest('/deck/list', token: token);
  }

  Future<Map<String, dynamic>> updateDeck(
    String token,
    String deckId,
    String name,
    List<String> cardIds,
  ) async {
    return await makeRequest(
      '/deck/update',
      method: 'PUT',
      token: token,
      body: {'deck_id': deckId, 'name': name, 'card_ids': cardIds},
    );
  }

  Future<Map<String, dynamic>> deleteDeck(String token, String deckId) async {
    return await makeRequest(
      '/deck/delete',
      method: 'DELETE',
      token: token,
      body: {'deck_id': deckId},
    );
  }

  // Game methods
  Future<Map<String, dynamic>> createSession(String token) async {
    return await makeRequest(
      createSessionEndpoint,
      method: 'POST',
      token: token,
    );
  }

  Future<Map<String, dynamic>> joinSession(
    String token,
    String sessionId,
  ) async {
    return await makeRequest(
      joinSessionEndpoint,
      method: 'POST',
      token: token,
      body: {'session_id': sessionId},
    );
  }

  Future<Map<String, dynamic>> selectDeck(
    String token,
    String sessionId,
    String deckId,
  ) async {
    return await makeRequest(
      '/session/select-deck',
      method: 'POST',
      token: token,
      body: {'session_id': sessionId, 'deck_id': deckId},
    );
  }

  Future<Map<String, dynamic>> getSessions(String token) async {
    return await makeRequest(getSessionsEndpoint, token: token);
  }

  Future<Map<String, dynamic>> startBattle(
    String token,
    String sessionId,
    String deckId,
  ) async {
    return await makeRequest(
      startBattleEndpoint,
      method: 'POST',
      token: token,
      body: {'session_id': sessionId, 'deck_id': deckId},
    );
  }

  Future<Map<String, dynamic>> getBattleState(
    String token,
    String sessionId,
  ) async {
    return await makeRequest(
      '$getBattleStateEndpoint?session_id=$sessionId',
      token: token,
    );
  }

  Future<Map<String, dynamic>> playCard(
    String token,
    String sessionId,
    String userId,
    int handIndex,
  ) async {
    return await makeRequest(
      playCardEndpoint,
      method: 'POST',
      token: token,
      body: {
        'session_id': sessionId,
        'user_id': userId,
        'hand_index': handIndex,
      },
    );
  }

  Future<Map<String, dynamic>> attack(
    String token,
    String sessionId,
    String userId,
    int attackerIndex,
    int targetIndex,
  ) async {
    return await makeRequest(
      attackEndpoint,
      method: 'POST',
      token: token,
      body: {
        'session_id': sessionId,
        'user_id': userId,
        'attacker_index': attackerIndex,
        'target_index': targetIndex,
      },
    );
  }

  Future<Map<String, dynamic>> endTurn(
    String token,
    String sessionId,
    String userId,
  ) async {
    return await makeRequest(
      endTurnEndpoint,
      method: 'POST',
      token: token,
      body: {'session_id': sessionId, 'user_id': userId},
    );
  }

  // Health check
  Future<Map<String, dynamic>> healthCheck() async {
    return await makeRequest(healthEndpoint);
  }

  // Leave session
  Future<Map<String, dynamic>> leaveSession(
    String token,
    String sessionId,
  ) async {
    return await makeRequest(
      '/session/leave',
      method: 'POST',
      token: token,
      body: {'session_id': sessionId},
    );
  }
}
