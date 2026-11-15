# Galaksi — rastgele sample toplayıcı

Galaksi, internetten rastgele sample toplayan bir VST3/AU plugin'i. İki kaynağı var, üstteki **kaynak** anahtarıyla seçiliyor:

- **Radyo:** internet radyolarını rastgele dinler, istediğin uzunlukta ve sayıda sample alır. Bütün sample'lar **aynı scale'e** taşınır: her kaydın tonu ve akortu tespit edilir, sonra kayıt hedef scale'in notalarına kaydırılır.
- **Perküsyon:** Freesound.org'dan **tek vuruşluk perküsif sesler** toplar: darbuka, tabla, taiko, bendir gibi etnik davullar, örs, metal boru, gong, kapı çarpması, tahta… Modern davul sesleri (808, kick, snare, hihat, synth…) dışarıda bırakılır. Her yenilemede farklı 16 ses gelir.

Sample'lar WAV olarak bir klasöre kaydedilir. **.adg tutamağı** onlardan hazır bir Ableton Drum Rack preset'i oluşturur; tutamağı Ableton'a sürükleyip bırakman yeterli.

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

## Perküsyon modu

1. **Freesound API anahtarı al (bir kez, ücretsiz):** freesound.org'da hesap aç, sonra https://freesound.org/apiv2/apply adresinden bir anahtar oluştur. Anahtarı **freesound api anahtarı** kutusuna yapıştır. Anahtar bu bilgisayarda saklanır, her projede tekrar girmen gerekmez.
2. **Arama kelimeleri:** Freesound İngilizce etiketlerle aranıyor, bu yüzden kelimeler İngilizce. Varsayılanlar: `darbuka, djembe, tabla, bendir, frame drum, daf, taiko, cajon, udu, talking drum, gong, anvil, metal hit, pipe hit, bell hit, pot lid, pan hit, door slam, door knock, wood block, wood hit, bucket hit, glass hit, stone hit`. Her yuva bunlardan birini rastgele seçer, art arda aynı kelimeyi seçmemeye çalışır.
3. **Hariç tut:** adında ya da etiketlerinde bu kelimeler geçen sesler alınmaz. Varsayılanlar: `808, 909, 707, drum machine, electronic, synth, edm, trap, dubstep, techno, kick, snare, hihat, hi-hat, cymbal, drum kit, drumkit, drum set, loop, voice, speech`.
4. **Lisans:** *hepsi* ya da *sadece cc0*. CC0 sesleri yazar adı belirtmeden her yerde kullanabilirsin. CC-BY seslerde yayınlarken yazar adını belirtmen gerekiyor; bunun için klasöre bir `KAYNAKLAR.txt` yazılıyor (dosya adı, ses adı, yazar, lisans, Freesound adresi).
5. **Maks uzunluk:** gong, zil gibi uzun kuyruklu sesler bu noktada yumuşak bir fade ile kesilir.
6. **KİLİTSİZLERİ YENİLE**'ye bas.

Her ses indirildikten sonra analiz edilir. Sadece gerçekten tek vuruş olanlar kabul edilir:
- Atak hızlı olmalı: başlangıçtan tepeye 60 ms'den kısa. Pad, drone ve uğultu bu yüzden elenir.
- Belirgin tek vuruş olmalı. Ritim ve loop elenir. Kayıtta ikinci bir vuruş varsa ve ilki o gelmeden sönmüşse, ilk vuruş kesilip kullanılır.
- Ses sönmeli. Sürekli devam eden sesler elenir.
- Baştaki sessizlik kırpılır, ses -0.5 dBFS'e normalize edilir.

Aynı ses bir oturumda iki kez gelmez. Arama sonuçlarından rastgele sayfalar seçildiği için her yenilemede farklı sesler çıkar. Freesound'un "tek olay" analizi de filtre olarak kullanılıyor.

## Drum Rack (.adg)

Sağ alttaki **drum rack'e sürükle (.adg)** tutamağını tut ve Ableton'da boş bir MIDI track'e (ya da track listesinin altındaki boşluğa) bırak. Plugin o anda hazır yuvalardan bir Drum Rack preset'i (`Galaksi_Perkusyon_…adg` ya da `Galaksi_Radyo_…adg`) oluşturup WAV klasörüne kaydeder. Yuva 1 → C1, yuva 2 → C#1… Pad'lerin köşesinde hangi notaya düşecekleri yazar. Boş yuvaların pad'i boş kalır.

`.adg` dosyası sample'ları kendi klasöründe arar, bu yüzden WAV'lar ile aynı klasörde durmalı. Klasörü taşıyacaksan ikisini birlikte taşı.

Yedek yol: **wav'lar** tutamağı bütün WAV'ları birden sürükler. Boş bir Drum Rack'in C1 pad'ine bırakırsan art arda pad'lere dizilirler.

## Radyo modu

1. **Türler:** Radio-Browser etiketleri, virgülle ayrılır. Örnek: `jazz, world music`, `afrobeat, turkish folk`, `ambient`.
2. **Hedef scale:** kök nota ve mod (Majör, Minör, Dorian, Frigyen, Lidyen, Miksolidyen, Lokriyen, Armonik minör).
3. **KİLİTSİZLERİ YENİLE**'ye bas. Her yuva ayrı ayrı rastgele bir istasyonu dinler, uygun bir kesit bulana kadar dener ve sonucu WAV olarak kaydeder.
4. Pad'ler (iki modda da):
   - **Tıkla:** sample'ı dinle. Tekrar tıklayınca durur.
   - **Sürükle:** WAV dosyasını Ableton'a (clip slot, Simpler, Drum Rack pad'i) bırak.
   - **KİLİT:** bu yuva yenilenmez.
   - **YENİ:** sadece bu yuvayı yenile. Arama sürerken düğme **DUR** olur.
5. **KLASÖRÜ AÇ** kaydedilen WAV'ları Finder'da gösterir. Varsayılan klasör `~/Music/Galaksi`. **KLASÖR…** ile değiştirilebilir.

Dosyaların üzerine asla yazılmaz. Her yenilemede yeni bir dosya oluşur, böylece Ableton'a sürüklediğin eski sample'lar bozulmaz. Dosya adında yuva numarası, hedef scale, orijinal ton, kaydırma ve istasyon adı yazar. Örnek: `03_Cminor_Emajor_-1st_Radio-Jazz_101950.wav`.

## Ayarlar

| Ayar | Ne yapar |
|---|---|
| Sample sayısı | 1–16 yuva |
| Uzunluk | radyo: sample süresi (0.1–16 sn). Perküsyon: en uzun sample, kuyruk burada kesilir |
| Dinleme süresi | her istasyondan kaç saniye kaydedilip analiz edilir. Uzun = daha isabetli ton tespiti, ama daha yavaş. Kesit, kaydın en tonal bölümünden alınır. |
| Ton güveni | seçicilik. Yüksek = sadece tonu çok net kayıtlar kabul edilir (konuşma ve reklamlar elenir), ama bulmak daha uzun sürer. |
| Maks kaydırma | en fazla kaç yarım ton kaydırılsın. Düşük tutarsan ses daha doğal kalır, ama daha az kayıt uyar. |
| Transpoz | **Kaliteli:** süre korunur (Rubber Band). **Bant:** plak/bant gibi, hız da değişir. |
| Önizleme sesi | plugin içindeki dinleme seviyesi |

Ayarların, kilitlerin ve yuvalardaki sample'ların hepsi Ableton projesiyle birlikte kaydedilir.

## Nasıl çalışıyor (radyo)

1. Radio-Browser'dan seçilen türde, çalışan MP3 istasyonları rastgele sırayla alınır.
2. İstasyon birkaç saniye dinlenir ve MP3 çözülür.
3. Ton tespiti: spektral tepe noktalarından chroma çıkarılır ve Krumhansl ton profilleriyle karşılaştırılır. A=440'tan akort sapması da ölçülür.
4. Ton net değilse (konuşma, gürültü) ya da gereken kaydırma sınırı aşıyorsa istasyon atlanır.
5. Kaydın en tonal bölümü seçilir. Bu bölüm, nota kümesi hedef scale'inkiyle aynı olacak kadar kaydırılır, akort da düzeltilir.

Not: majör bir kayıt, göreli minörüyle aynı notaları paylaşır. Bu yüzden C minör hedefinde bir sample Eb majör olarak da gelebilir. Notalar aynıdır, sadece merkezi farklıdır.

## Sınırlar

- Sadece MP3 yayınları kullanılıyor (istasyonların çoğu MP3). AAC, OGG ve HLS istasyonları atlanıyor.
- Perküsyon modu Freesound'un önizleme dosyalarını (128 kbps MP3) kullanıyor. Orijinal dosyaları indirmek için Freesound kullanıcı girişi (OAuth) gerekiyor. Tek vuruşluk seslerde fark pek duyulmaz.
- Freesound'un günlük istek sınırı var (yaklaşık 2000 arama). Normal kullanımda sorun olmaz.
- Radyo içeriği telifli. Kişisel kullanım ve deney için sorun yok, ama yayınlayacağın parçalarda tanınabilir kesitlere dikkat et.

## Derleme

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build        # çekirdek testleri
```

JUCE 8 ve Rubber Band derleme sırasında otomatik indirilir. Rubber Band GPL lisanslı olduğu için bu proje de GPL lisanslıdır.
