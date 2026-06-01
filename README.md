# Raptor
 
A C++ C2 (command and control) server with multiple protocols (TCP/UDP, etc.), session management, packet parsing, and a UI interface to manage sessions, servers, and more.

## Building
Dependencies are managed via setup.sh. The build will intentionally fail on first configure if deps are missing, that's expected. The CMakeLists.txt is located in the core/. 

**1. Configure (will faild if deps missing)**
```bash
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
```
 
**2. Install dependencies**
```bash
cmake --build build/debug --target setup
```
 
**3. Reconfigure**
```bash
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
```
 
**4. Compile**
```bash
cmake --build build/debug -j$(nproc)
```
this binary in /build/debug/raptor
