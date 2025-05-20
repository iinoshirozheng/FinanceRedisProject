[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/iinoshirozheng/FinanceRedisProject)
# Finance System

A C++ application for processing finance bills over TCP and maintaining margin and short selling data.

## Project Structure

The project follows a clean architecture approach with the following layers:

- **Domain Layer**: Core business logic and interfaces
- **Application Layer**: Use cases and service implementations
- **Infrastructure Layer**: External systems adapters (Redis, TCP, etc.)
- **Entry Point**: Application wiring and startup

```
project_root/
├── library/
│   ├── domain/                 // Domain entities and interfaces
│   ├── application/            // Business logic implementation
│   ├── infrastructure/         // External system adapters
│   │   ├── network/            // TCP server implementation
│   │   ├── storage/            // Redis and file storage adapters
│   │   └── logging/            // Logging utilities
│   └── entry/                  // Application entry point
├── third_party/                // Third-party dependencies
├── connection.json             // Redis and server configuration
├── area_branch.json            // Area-branch mapping configuration
├── CMakeLists.txt              // Build configuration
└── build.sh                    // Build script
```

## Building the Project

### Prerequisites

- CMake 3.14 or higher
- C++17 compatible compiler
- Poco library
- nlohmann_json
- fmt
- redis++
- Boost (algorithm)

### Build Steps

Using the build script:
```bash
./build.sh
```

Or manually:
```bash
mkdir -p build
cd build
cmake ..
make
```

The executable will be generated in the `build/bin` directory.

## Running the Application

The finance system requires configuration in two JSON files:

1. `connection.json`: Redis connection and server port settings
2. `area_branch.json`: Area-branch mapping configuration

Example `connection.json`:
```json
{
  "redis_url": "tcp://127.0.0.1:6479",
  "server_port": 9516
}
```

Example `area_branch.json`:
```json
{
  "A01": ["B101", "B102"],
  "A02": ["B201", "B202"]
}
```

To run the application:
```bash
./build/bin/finance_app
```

To initialize Redis search indices on startup:
```bash
./build/bin/finance_app --init-indices
```

## Design Principles

This project follows:

- SOLID principles
- Clean Architecture
- Dependency Injection
- Separation of Concerns

## License

Proprietary 
