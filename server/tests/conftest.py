# NOTE: Requires pytest and yandex-taxi-testsuite to be installed in your Python environment.
import pytest
import os
import atexit

pytest_plugins = ['pytest_userver.plugins.core']

# Global variable to track test database path
test_db_path = None

@pytest.fixture(scope='session')
def service_binary():
    # Path to the built server binary (adjust if needed)
    return os.path.abspath(os.path.join(os.path.dirname(__file__), '../build/debug/Server'))

@pytest.fixture(scope='session')
def service_args():
    # Use the test config for the server
    config_path = os.path.abspath(os.path.join(os.path.dirname(__file__), '../configs/static_config_test.yaml'))
    return ['--config', config_path]

@pytest.fixture(scope='session')
def service_env():
    # Set environment variables for the test DB if needed
    global test_db_path
    env = os.environ.copy()
    env['LOG_LEVEL'] = 'error'
    test_db_path = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../../test_cardbattle.db'))
    env['TEST_DB_PATH'] = test_db_path
    return env

def cleanup_test_db():
    """Clean up test database after tests finish"""
    global test_db_path
    if test_db_path and os.path.exists(test_db_path):
        try:
            os.remove(test_db_path)
            print(f"Cleaned up test database: {test_db_path}")
        except Exception as e:
            print(f"Failed to clean up test database: {e}")

# Register cleanup function to run at exit
atexit.register(cleanup_test_db)

@pytest.fixture(scope='session', autouse=True)
def cleanup_after_tests():
    """Clean up test database after all tests finish"""
    yield  # Run tests
    cleanup_test_db()  # Clean up after tests 