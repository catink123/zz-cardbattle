# Card Battle Client

A Flutter client for the Card Battle game, designed to work with the updated server architecture.

## Features

- **Authentication**: Register and login with username/password
- **Session Management**: Create and join game sessions
- **Real-time Battle**: WebSocket-based real-time gameplay
- **Modern UI**: Clean, responsive interface with Material Design

## Architecture

The client follows a clean architecture pattern:

- **Services**: API and WebSocket communication
- **Providers**: State management with Provider pattern
- **Screens**: UI components for different app states
- **Models**: Data structures (if needed)

## Key Changes

### Server Integration
- Updated to work with the new server endpoints
- HTTP for session management only
- WebSocket for real-time battle communication
- Removed deck management (simplified for MVP)

### Authentication
- Token-based authentication
- Automatic token storage and retrieval
- User session management

### Session Management
- Create sessions via HTTP
- Join sessions via HTTP
- Leave sessions via HTTP
- View available sessions

### Real-time Battle
- WebSocket connection for battle sessions
- Real-time game state updates
- Action sending (play card, attack, end turn)
- Connection status monitoring

## Getting Started

### Prerequisites
- Flutter SDK (3.8.1 or higher)
- Dart SDK
- Android Studio / VS Code

### Installation

1. Clone the repository
2. Navigate to the client directory
3. Install dependencies:
   ```bash
   flutter pub get
   ```

### Running the App

1. Ensure the server is running on `localhost:8080`
2. Run the Flutter app:
   ```bash
   flutter run
   ```

## Usage

### Authentication
1. Launch the app
2. Register with username, email, and password
3. Or login with existing credentials
4. You'll be redirected to the home screen

### Creating a Session
1. On the home screen, tap "Create Session"
2. The session ID will be displayed
3. Share this ID with another player

### Joining a Session
1. On the home screen, tap "Join Session"
2. Enter the session ID provided by the host
3. Or tap "Join" on an available session in the list

### Battle
1. Once in a session, the battle screen will appear
2. Wait for both players to join
3. Use the action buttons to play cards, attack, and end turns
4. Real-time updates will appear as the battle progresses

## Development

### Project Structure
```
lib/
├── main.dart              # App entry point
├── services/
│   ├── api_service.dart   # HTTP API communication
│   └── websocket_service.dart # WebSocket communication
├── providers/
│   ├── auth_provider.dart # Authentication state
│   └── game_provider.dart # Game state management
├── screens/
│   ├── login_screen.dart  # Login/register screen
│   ├── home_screen.dart   # Main menu
│   └── battle_screen.dart # Battle interface
└── widgets/               # Reusable UI components
```

### Key Dependencies
- `provider`: State management
- `http`: HTTP API calls
- `web_socket_channel`: WebSocket communication
- `shared_preferences`: Local storage for tokens

## Troubleshooting

### Connection Issues
- Ensure the server is running on `localhost:8080`
- Check that the server endpoints match the client expectations
- Verify WebSocket connection in the battle screen

### Authentication Issues
- Clear app data if token issues occur
- Check server logs for authentication errors
- Ensure username/email uniqueness during registration

### Battle Issues
- Check WebSocket connection status
- Verify session IDs are correct
- Monitor server logs for battle-related errors

## Future Enhancements

- Deck building interface
- Card collection management
- Advanced battle animations
- Chat functionality
- Leaderboards and statistics
- Sound effects and music
