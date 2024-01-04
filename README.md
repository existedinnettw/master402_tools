
# master402 tools

small tools which may help master402 development e.g. ethercat operation.

- pdo_mapping: igh ethercat pdo mapping.

Originally, this is part of master402.

## compile

```bash
conan install . --output-folder=build -o capehand/*:with_igh=True --build=missing
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build .
```
