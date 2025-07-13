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

  // Global navigator key for safe navigation
  static final GlobalKey<NavigatorState> navigatorKey =
      GlobalKey<NavigatorState>();

  @override
  Widget build(BuildContext context) {
    return MultiProvider(
      providers: [
        ChangeNotifierProvider(create: (_) => AuthProvider()),
        ChangeNotifierProvider(create: (_) => GameProvider()),
        Provider<ApiService>(create: (_) => ApiService()),
      ],
      child: MaterialApp(
        navigatorKey: navigatorKey,
        title: 'Card Battle',
        theme: ThemeData.dark().copyWith(
          colorScheme: ThemeData.dark().colorScheme.copyWith(
            primary: Colors.blue,
            secondary: Colors.blueAccent,
          ),
          scaffoldBackgroundColor: const Color(0xFF18192A),
          cardColor: const Color(0xFF23243A),
          appBarTheme: const AppBarTheme(
            backgroundColor: Color(0xFF23243A),
            foregroundColor: Colors.white,
            elevation: 0,
          ),
          snackBarTheme: const SnackBarThemeData(
            backgroundColor: Colors.black87,
            contentTextStyle: TextStyle(color: Colors.white),
          ),
        ),
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
