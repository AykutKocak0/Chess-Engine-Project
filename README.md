## Derleme ve Çalıştırma Kılavuzu (Compilation Guide)

Bu proje, C++ satranç motorunun kök dizindeki kaynak kodlarından derlenmesiyle çalışır.

### 1. Motorun Derlenmesi (C++ Engine Compilation)

Kaynak kodları herhangi bir modern C++ derleyicisi (GCC, Clang veya MSVC) ile derleyebilirsiniz.

**GCC (MinGW / Linux) ile Terminalden Derleme:**
Projenin kök dizininde bir terminal açın ve şu komutu çalıştırın:
```bash
g++ -O3 -std=c++17 *.cpp -o Botkut
