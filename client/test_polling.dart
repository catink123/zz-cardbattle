import 'dart:async';
import 'dart:convert';
import 'package:http/http.dart' as http;

class PollingTest {
  static const String baseUrl = 'http://localhost:8080';
  static const String testUser1 = 'test_user_1';
  static const String testUser2 = 'test_user_2';

  static Future<void> main() async {
    print('Starting polling test...');

    // Test 1: Create a session
    print('\n1. Creating session...');
    final sessionId = await createSession(testUser1);
    print('Session created: $sessionId');

    // Test 2: Check if session appears in list
    print('\n2. Checking session list...');
    final sessions = await getSessions(testUser2);
    print('Found ${sessions.length} sessions');
    for (final session in sessions) {
      print(
        '  - Session ${session['id']}: ${session['status']} (host: ${session['host_id']})',
      );
    }

    // Test 3: Join session
    print('\n3. Joining session...');
    await joinSession(testUser2, sessionId);

    // Test 4: Check updated session list
    print('\n4. Checking updated session list...');
    final updatedSessions = await getSessions(testUser1);
    print('Found ${updatedSessions.length} sessions');
    for (final session in updatedSessions) {
      print(
        '  - Session ${session['id']}: ${session['status']} (host: ${session['host_id']}, guest: ${session['guest_id']})',
      );
    }

    print('\nPolling test completed successfully!');
  }

  static Future<String> createSession(String userId) async {
    final response = await http.post(
      Uri.parse('$baseUrl/game/create-session'),
      headers: {'Authorization': 'Bearer $userId'},
    );

    if (response.statusCode == 200) {
      final data = jsonDecode(response.body);
      return data['session_id'];
    } else {
      throw Exception('Failed to create session: ${response.body}');
    }
  }

  static Future<List<Map<String, dynamic>>> getSessions(String userId) async {
    final response = await http.get(
      Uri.parse('$baseUrl/game/sessions'),
      headers: {'Authorization': 'Bearer $userId'},
    );

    if (response.statusCode == 200) {
      final data = jsonDecode(response.body);
      return List<Map<String, dynamic>>.from(data['sessions'] ?? []);
    } else {
      throw Exception('Failed to get sessions: ${response.body}');
    }
  }

  static Future<void> joinSession(String userId, String sessionId) async {
    final response = await http.post(
      Uri.parse('$baseUrl/game/join-session'),
      headers: {
        'Authorization': 'Bearer $userId',
        'Content-Type': 'application/json',
      },
      body: jsonEncode({'session_id': sessionId}),
    );

    if (response.statusCode != 200) {
      throw Exception('Failed to join session: ${response.body}');
    }
  }
}

void main() {
  PollingTest.main();
}
