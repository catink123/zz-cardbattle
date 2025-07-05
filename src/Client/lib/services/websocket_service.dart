import 'dart:convert';
import 'dart:async';
import 'package:web_socket_channel/web_socket_channel.dart';
import 'package:web_socket_channel/status.dart' as status;

class WebSocketService {
  WebSocketChannel? _channel;
  StreamController<Map<String, dynamic>>? _messageController;
  bool _isConnected = false;
  String? _sessionId;
  String? _userId;

  // Getters
  bool get isConnected => _isConnected;
  Stream<Map<String, dynamic>>? get messageStream => _messageController?.stream;

  // Connect to battle WebSocket
  Future<bool> connectToBattle(String sessionId, String userId) async {
    try {
      _sessionId = sessionId;
      _userId = userId;

      print(
        'Connecting to WebSocket with sessionId: $sessionId, userId: $userId',
      );

      // Create WebSocket connection
      final uri = Uri.parse(
        'ws://localhost:8080/battle/ws?session_id=$sessionId&user_id=$userId',
      );
      print('WebSocket URI: $uri');

      _channel = WebSocketChannel.connect(uri);

      // Create message controller
      _messageController = StreamController<Map<String, dynamic>>.broadcast();

      // Listen for messages
      _channel!.stream.listen(
        (message) {
          print('Raw WebSocket message received: $message');
          try {
            final data = jsonDecode(message);
            print('Parsed WebSocket message: $data');
            _messageController?.add(data);
          } catch (e) {
            print('Error parsing WebSocket message: $e');
          }
        },
        onError: (error) {
          print('WebSocket error: $error');
          _isConnected = false;
          _messageController?.addError(error);
        },
        onDone: () {
          print('WebSocket connection closed');
          _isConnected = false;
          _messageController?.close();
        },
      );

      _isConnected = true;
      print('WebSocket connected to battle session: $sessionId');
      return true;
    } catch (e) {
      print('Error connecting to WebSocket: $e');
      _isConnected = false;
      return false;
    }
  }

  // Send a message
  void sendMessage(Map<String, dynamic> message) {
    if (_isConnected && _channel != null) {
      try {
        final jsonMessage = jsonEncode(message);
        _channel!.sink.add(jsonMessage);
      } catch (e) {
        print('Error sending WebSocket message: $e');
      }
    } else {
      print('WebSocket not connected');
    }
  }

  // Send play card action
  void playCard(int handIndex) {
    sendMessage({
      'type': 'play_card',
      'session_id': _sessionId,
      'user_id': _userId,
      'hand_index': handIndex,
    });
  }

  // Send attack action
  void attack(int attackerIndex, int targetIndex) {
    if (_channel != null) {
      _channel!.sink.add(
        jsonEncode({
          'type': 'attack',
          'attacker_index': attackerIndex,
          'target_index': targetIndex,
        }),
      );
    }
  }

  void endTurn() {
    if (_channel != null) {
      _channel!.sink.add(jsonEncode({'type': 'end_turn'}));
    }
  }

  // Request battle state
  void requestBattleState() {
    sendMessage({
      'type': 'get_battle_state',
      'session_id': _sessionId,
      'user_id': _userId,
    });
  }

  // Disconnect
  void disconnect() {
    print('WebSocketService: Disconnecting...');
    _isConnected = false;

    try {
      if (_channel != null) {
        _channel!.sink.close(status.goingAway);
        _channel = null;
      }
    } catch (e) {
      print('WebSocketService: Error closing channel: $e');
    }

    try {
      if (_messageController != null) {
        _messageController!.close();
        _messageController = null;
      }
    } catch (e) {
      print('WebSocketService: Error closing message controller: $e');
    }

    _sessionId = null;
    _userId = null;
    print('WebSocketService: Disconnected successfully');
  }

  // Dispose
  void dispose() {
    print('WebSocketService: Disposing...');
    disconnect();
  }
}
