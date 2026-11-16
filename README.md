# Botkut

Botkut, C++17 ile yazılmış, UCI protokolünü konuşan bir satranç motorudur. Yanında motora karşı oynamak için Godot ile yazılmış bir görselleştirici (GUI) gelir.

## Özellikler

**Tahta ve hamle üretimi**
- Bitboard tabanlı tahta gösterimi
- Kaleler ve filler için magic bitboard'lar
- Zobrist hash (tahta ve ayrı piyon anahtarı)
- Perft ile doğrulanmış hamle üretimi (standart test pozisyonlarının hepsinde doğru sonuç verir)

**Arama**
- Iterative deepening ve aspiration window
- Principal Variation Search (PVS)
- Transposition table (64 MB)
- Null move pruning, Late Move Reductions (LMR), razoring, Internal Iterative Reduction
- Şah çekmede derinlik uzatma (check extension)
- Hamle sıralaması: hash hamlesi, SEE ile yakalamalar, killer hamleler, countermove, history ve continuation history
- Quiescence search (yakalamalar ve sessiz şahlar, SEE ile budama)
- Tekrar ve 50 hamle kuralı tespiti
- Kalan süre ve artışa göre zaman yönetimi

**Değerlendirme**
- Oyun evresine göre geçişli (tapered) değerlendirme
- Taş-kare tabloları (PST)
- Piyon yapısı (çift, izole, geçer piyon) ve piyon hash tablosu
- Taş hareketliliği (mobility), şah güvenliği, fil çifti
- Tehditler, açmazdaki taşlar, sıkışmış taş kalıpları
- Oyun sonunda mop-up değerlendirmesi ve bilinen berabere materyal tespiti

## Klasör Yapısı

```
Chess-Engine-Project/
├── Chess Engine/              # C++ motor kaynak kodu
│   ├── main.cpp               # UCI döngüsü
│   ├── Board.*                # Tahta, make/unmake, FEN, SEE
│   ├── GenerateMoves.*        # Hamle üretimi, saldırı tabloları
│   ├── Search.*               # Arama
│   ├── EngineEval.*           # Değerlendirme
│   ├── EvalParams.*           # Değerlendirme ağırlıkları (tuning için tek yerde)
│   ├── TransPositionTable.*   # Hash tabloları
│   ├── TimeManagement.*       # Zaman yönetimi
│   ├── Constants.*            # Zobrist anahtarları, maskeler, LMR tablosu
│   ├── Magics.h               # Hazır magic sayıları
│   ├── MagicCreator.c         # Magic sayılarını üreten ayrı araç (motora derlenmez)
│   └── CMakeLists.txt
└── Chess Engine Visualizer/   # Godot 4.6 görselleştirici projesi
```

## Derleme

Herhangi bir modern C++17 derleyicisi (GCC, Clang veya MSVC) yeterlidir. Komutları `Chess Engine` klasörünün içinde çalıştırın.

### GCC / MinGW

```bash
cd "Chess Engine"
g++ -O3 -std=c++17 *.cpp -o Botkut
```

Windows'ta MinGW ile derlerseniz, exe'nin MinGW DLL'leri olmadan da çalışması için `-static` eklemeniz önerilir:

```bash
g++ -O3 -std=c++17 -static *.cpp -o Botkut.exe
```

### CMake

```bash
cd "Chess Engine"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Kullanım

### Görselleştirici ile oynamak

1. Motoru derleyin.
2. Oluşan `Botkut.exe` dosyasını `Chess Engine Visualizer` klasörüne kopyalayın. Görselleştirici motoru önce kendi exe'sinin yanında, sonra proje klasöründe (`res://Botkut.exe`) arar.
3. `Chess Engine Visualizer` klasörünü Godot 4.6 ile açıp projeyi çalıştırın.

Görselleştiricide şunlar var:
- İnsan vs Bot ve İnsan vs İnsan modları
- Maksimum arama derinliği ayarı
- Tahtayı çevirme (siyahla oynama)
- FEN ile pozisyon yükleme
- Piyon terfisinde taş seçimi
- Değerlendirme göstergesi

> Not: Görselleştirici şu an yalnızca Windows'ta çalışır, çünkü motoru `Botkut.exe` adıyla arar.

### UCI arayüzü olarak

Botkut standart girdi/çıktı üzerinden UCI konuştuğu için Cute Chess, Arena, BanksiaGUI gibi UCI destekli arayüzlere motor olarak eklenebilir.

Desteklenen komutlar:

| Komut | Açıklama |
|---|---|
| `uci`, `isready`, `ucinewgame`, `quit` | Standart UCI komutları |
| `position startpos [moves ...]` | Başlangıç pozisyonunu kurar |
| `position fen <FEN> [moves ...]` | FEN'den pozisyon kurar |
| `go depth N` | Belirtilen derinliğe kadar arar |
| `go wtime/btime/winc/binc/movestogo` | Süreye göre arar |
| `go movetime N` | Hamle başına sabit N ms arar |
| `go nodes N` | N düğüm aradıktan sonra durur |
| `go infinite` | `stop` gelene kadar arar |
| `stop` | Aramayı durdurur, motor `bestmove` ile cevap verir |
| `perft N` | N derinliğinde perft çalıştırır, düğüm sayısını ve hızı yazar |
| `eval` | Mevcut pozisyonun statik değerlendirmesini iki taraf için yazar |
| `checklegal <hamle>` | Hamlenin legal olup olmadığını yazar (görselleştirici kullanır) |
| `fen` | Mevcut pozisyonun FEN'ini yazar |
| `datagen <oyun> <düğüm> <dosya> [tohum] [oyun başına pozisyon]` | Kendi kendine oynayıp tuning verisi üretir (aşağıya bakın) |

### Veri üretimi (datagen)

Motor kendi kendine oynar ve oyunlardaki sakin pozisyonları oyun sonucuyla birlikte EPD olarak yazar. Her satır şöyle görünür:

```
<pozisyon> c9 "1-0"; ce 35;
```

`c9` beyaz açısından oyun sonucu, `ce` sıradaki taraf açısından arama skorudur. Her oyun 8-9 rastgele hamleyle başlar; böylece oyunlar birbirinden farklı olur. Komut satırından da çalıştırılabilir. Birden fazla çekirdek kullanmak için her süreçte farklı bir tohum ve dosya verin:

```bash
Botkut.exe datagen 10000 5000 data_1.epd 1
Botkut.exe datagen 10000 5000 data_2.epd 2
```

### Texel tuning

Datagen'in ürettiği dosyalarla `EvalParams` içindeki ağırlıklar ayarlanabilir. Tuner her ağırlığı ±1 değiştirip oyun sonuçlarıyla tahmin arasındaki ortalama kare hatayı düşürüyorsa değişikliği tutar:

```bash
Botkut.exe tune threads=8 data_1.epd data_2.epd data_3.epd
```

Seçenekler: `threads=N`, `max=N` (rastgele N pozisyon kullan), `iters=N`, `lambda=X` (1 = yalnızca oyun sonucu, 0 = yalnızca datagen arama skoru), `valid=X` (doğrulama payı, varsayılan 0.1), `patience=N` (varsayılan 3), `out=İSİM`. Materyal değerleri sabit tutulur; ayarı PST'ler taşır.

Her dosyanın son %10'u (yani bütün oyunlar) tuning'e katılmaz, yalnızca doğrulama hatasını ölçmek için kullanılır. Eğitim hatası düşerken doğrulama hatası düşmüyorsa ağırlıklar veriyi ezberlemeye başlamıştır; doğrulama hatası `patience` iterasyon boyunca yeni bir en iyi değere ulaşmazsa tuning durur.

Çıktı dosyaları:

| Dosya | İçerik |
|---|---|
| `tuned.txt`, `tuned_EvalParams.txt` | Güncel ağırlıklar, her 50 ağırlıkta bir kaydedilir |
| `tuned_iterN.txt` | N. iterasyon sonundaki ağırlıklar |
| `tuned_best.txt`, `tuned_best_EvalParams.txt` | Doğrulama hatası en düşük olan ağırlıklar (kullanılması önerilen) |

`.txt` dosyaları `params` ile yüklenebilir; `_EvalParams.txt` dosyaları `EvalParams.h`'ye yapıştırılabilir C++ kodudur.

Ayarlanmış değerlerle motoru yeniden derlemeden çalıştırmak veya tuning'e kaldığı yerden devam etmek için:

```bash
Botkut.exe params tuned_best.txt
Botkut.exe params tuned.txt tune threads=8 data_1.epd data_2.epd data_3.epd
```

Örnek:

```
position startpos moves e2e4 e7e5
go depth 10
```

## Bilinen Sınırlamalar

- `setoption` (örneğin Hash boyutu) ve ponder desteklenmiyor.
- PV, transposition table'dan sonuç alınan düğümlerde kısa kalabilir.

## Doğrulama

Hamle üretimini perft ile kontrol edebilirsiniz:

```
position startpos
perft 5
```

Beklenen sonuç `Nodes: 4865609` olmalıdır.
