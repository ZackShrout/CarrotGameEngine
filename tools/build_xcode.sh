cmake -S . -B build-xcode -G Xcode
cmake --build build-xcode --config Debug --target CarrotSandbox
open build-xcode/CarrotGameEngine.xcodeproj