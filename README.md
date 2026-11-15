# Galaksi — canlı radyo sampler

Galaksi, internet radyolarını rastgele dinleyip istediğin uzunlukta ve sayıda sample toplayan bir VST3/AU plugin'i. Bütün sample'lar **aynı scale'e** taşınır: her kaydın tonu ve akortu tespit edilir, sonra kayıt hedef scale'in notalarına kaydırılır. Sample'lar WAV olarak bir klasöre kaydedilir. Pad'den Ableton'a sürükleyip bırakabilirsin.

## Kurulum (Mac)

1. GitHub'da **Actions** sekmesine gir. En son yeşil (✓) çalışmayı aç, en alttaki `Galaksi-mac-…` paketini indir ve aç.
2. Dosyaları kopyala:
   - `Galaksi.vst3` → `~/Library/Audio/Plug-Ins/VST3/`
   - `Galaksi.component` → `~/Library/Audio/Plug-Ins/Components/`
3. Plugin imzasız olduğu için macOS ilk açılışta onu engelleyebilir. Terminal'de şu komutu bir kez çalıştır:
   ```
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Galaksi.vst3 ~/Library/Audio/Plug-Ins/Components/Galaksi.component
   ```
4. Ableton > Preferences > Plug-Ins bölümünde **Use VST3 Plug-in System Folders** açık olsun, sonra **Rescan**.
5. Bir MIDI track'e **Galaksi**'yi instrument olarak yükle.

Paket hem Apple Silicon hem Intel/Rosetta için derlenir. Live 10 M1'de Rosetta ile çalıştığı için bu önemli.

## Kullanım

1. **Türler:** Radio-Browser etiketleri, virgülle ayrılır. Örnek: `jazz, world music`, `afrobeat, turkish folk`, `ambient`.
2. **Hedef scale:** kök nota ve mod (Majör, Minör, Dorian, Frigyen, Lidyen, Miksolidyen, Lokriyen, Armonik minör).
3. **KİLİTSİZLERİ YENİLE**'ye bas. Her yuva ayrı ayrı rastgele bir istasyonu dinler, uygun bir kesit bulana kadar dener ve sonucu WAV olarak kaydeder.
4. Pad'ler:
   - **Tıkla:** sample'ı dinle. Tekrar tıklayınca durur.
   - **Sürükle:** WAV dosyasını Ableton'a (clip slot, Simpler, Drum Rack pad'i) bırak.
   - **KİLİT:** bu yuva yenilenmez.
   - **YENİ:** sadece bu yuvayı yenile. Arama sürerken düğme **DUR** olur.
   - MIDI ile de çalabilirsin: C1 = yuva 1, C#1 = yuva 2 … D#2 = yuva 16.
5. **KLASÖRÜ AÇ** kaydedilen WAV'ları Finder'da gösterir. Varsayılan klasör `~/Music/Galaksi`. **KLASÖR…** ile değiştirilebilir.

Dosyaların üzerine asla yazılmaz. Her yenilemede yeni bir dosya oluşur, böylece Ableton'a sürüklediğin eski sample'lar bozulmaz. Dosya adında yuva numarası, hedef scale, orijinal ton, kaydırma ve istasyon adı yazar. Örnek: `03_Cminor_Emajor_-1st_Radio-Jazz_101950.wav`.

## Ayarlar

| Ayar | Ne yapar |
|---|---|
| Sample sayısı | 1–16 yuva |
| Uzunluk | sample süresi (0.1–16 sn) |
| Dinleme süresi | her istasyondan kaç saniye kaydedilip analiz edilir. Uzun = daha isabetli ton tespiti, ama daha yavaş. Kesit, kaydın en tonal bölümünden alınır. |
| Ton güveni | seçicilik. Yüksek = sadece tonu çok net kayıtlar kabul edilir (konuşma ve reklamlar elenir), ama bulmak daha uzun sürer. |
| Maks kaydırma | en fazla kaç yarım ton kaydırılsın. Düşük tutarsan ses daha doğal kalır, ama daha az kayıt uyar. |
| Transpoz | **Kaliteli:** süre korunur (Rubber Band). **Bant:** plak/bant gibi, hız da değişir. |
| Önizleme sesi | plugin içindeki dinleme seviyesi |

Ayarların, kilitlerin ve yuvalardaki sample'ların hepsi Ableton projesiyle birlikte kaydedilir.

## Nasıl çalışıyor

1. Radio-Browser'dan seçilen türde, çalışan MP3 istasyonları rastgele sırayla alınır.
2. İstasyon birkaç saniye dinlenir ve MP3 çözülür.
3. Ton tespiti: spektral tepe noktalarından chroma çıkarılır ve Krumhansl ton profilleriyle karşılaştırılır. A=440'tan akort sapması da ölçülür.
4. Ton net değilse (konuşma, gürültü) ya da gereken kaydırma sınırı aşıyorsa istasyon atlanır.
5. Kaydın en tonal bölümü seçilir. Bu bölüm, nota kümesi hedef scale'inkiyle aynı olacak kadar kaydırılır, akort da düzeltilir.

Not: majör bir kayıt, göreli minörüyle aynı notaları paylaşır. Bu yüzden C minör hedefinde bir sample Eb majör olarak da gelebilir. Notalar aynıdır, sadece merkezi farklıdır.

## Sınırlar

- Sadece MP3 yayınları kullanılıyor (istasyonların çoğu MP3). AAC, OGG ve HLS istasyonları atlanıyor.
- Radyo içeriği telifli. Kişisel kullanım ve deney için sorun yok, ama yayınlayacağın parçalarda tanınabilir kesitlere dikkat et.

## Derleme

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build        # çekirdek testleri
```

JUCE 8 ve Rubber Band derleme sırasında otomatik indirilir. Rubber Band GPL lisanslı olduğu için bu proje de GPL lisanslıdır.
