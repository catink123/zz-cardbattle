import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'screens/login_screen.dart';
import 'screens/home_screen.dart';
import 'screens/battle_screen.dart';
import 'services/api_service.dart';
import 'providers/auth_provider.dart';
import 'providers/game_provider.dart';

void main() {
  runApp(const CardBattleApp());
}

class CardBattleApp extends StatelessWidget {
  const CardBattleApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MultiProvider(
      providers: [
        ChangeNotifierProvider(create: (_) => AuthProvider()),
        ChangeNotifierProvider(create: (_) => GameProvider()),
        Provider<ApiService>(create: (_) => ApiService()),
      ],
      child: MaterialApp(
        title: 'Card Battle',
        theme: ThemeData(primarySwatch: Colors.blue, useMaterial3: true),
        initialRoute: '/login',
        routes: {
          '/login': (context) => const LoginScreen(),
          '/home': (context) => const HomeScreen(),
          '/battle': (context) {
            final args = ModalRoute.of(context)?.settings.arguments;
            if (args is String) {
              return BattleScreen(sessionId: args);
            }
            return const Scaffold(
              body: Center(child: Text('No session ID provided')),
            );
          },
        },
      ),
    );
  }
}
