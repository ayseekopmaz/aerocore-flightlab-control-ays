# AeroCore — Çoklu Drone Simülasyon ve Test Platformu

AeroCore, **PX4 SITL + Gazebo** uçuşlarını Windows üzerinde **WSL2** aracılığıyla başlatan ve izleyen bir **C++20 / Qt 6** masaüstü uygulamasıdır. Operatör, aynı panelden en fazla üç x500 drone için senaryo oluşturabilir, uçuşu takip edebilir, sonuçları karşılaştırabilir ve oturum kaydını yeniden oynatabilir.

> **Durum:** Araştırma ve geliştirme amaçlı simülasyon yazılımı. Gerçek araca bağlanma veya uçuşa elverişlilik doğrulaması bu sürümün kapsamı değildir.

## Neler yapıyor?

- **Çoklu drone:** Tek Gazebo dünyasında üç ayrı PX4 SITL oturumu; araç başına sistem kimliği, profil ve telemetri.
- **Canlı konsol:** Konum, irtifa, tutum, batarya ve bağlantı durumu; harita ve üç araç için renkli uçuş izleri.
- **3B görev sahnesi:** MAVLink telemetrisinden üretilen döndürülebilir/yakınlaştırılabilir yerel koordinat görünümü. Pembe, kırmızı ve sarı araçları ayırır; arayüz açık mavi/laciverttir.
- **Deney laboratuvarı:** Hover, rota, kalkış/iniş, rüzgâr, yük ve ağırlık merkezi, batarya, sensör, bağlantı, kontrol parametresi ve regresyon senaryoları. Eşikler, olay zamanı ve bazı koşullar kullanıcı tarafından belirlenir.
- **Ölçüm ve rapor:** Senaryo sonucu `PASS`, `FAIL` veya `INCONCLUSIVE`; planlanan/gerçekleşen rota, zaman serisi, olay kaydı ve JSON/CSV/HTML dışa aktarımı. Aynı senaryoda araçlar **sırayla** karşılaştırılır.
- **Oturum oynatma:** Kaydedilen telemetrinin harita ve 3B sahne üzerinde zaman çubuğuyla yeniden izlenmesi.
- **İsteğe bağlı video:** Qt Multimedia kuruluysa operatör yerel video dosyası veya RTSP/HTTP kaynağı açabilir.

## Gereksinimler

| Bileşen | Gereksinim |
|---|---|
| Masaüstü | Windows 10/11, Qt 6.4+ (Core, Gui, Widgets, Network), C++20 derleyici, CMake ve Ninja |
| Simülasyon | WSL2 içinde Ubuntu, PX4-Autopilot kaynakları ve uyumlu Gazebo kurulumu |
| Video (isteğe bağlı) | Qt Multimedia ve Multimedia Widgets |
| Ağ | WSL2 NAT; panelin kullandığı UDP 14560 portu boş olmalı |

**AeroCore, WSL/PX4/Gazebo'yu kurmaz.** Otomatik başlatıcı Windows + WSL2 NAT içindir. Linux üzerinde arayüz ve testler derlenebilir; yerel Linux PX4 başlatıcısı bu sürümde bulunmaz.

Kurulum kaynakları: [Microsoft WSL](https://learn.microsoft.com/en-us/windows/wsl/install) ve [PX4'ün WSL2 geliştirme kılavuzu](https://docs.px4.io/main/en/dev_setup/dev_env_windows_wsl).

## Hızlı başlangıç

1. Bu depoyu indirin ve `CMakeLists.txt` dosyasını **Qt Creator** ile açın.
2. Qt 6 ve C++20 destekli kitinizi seçin; **Build**, ardından **Run** yapın.
3. İlk açılışta WSL dağıtımı ve PX4 kaynak klasörü aranır. Üstteki alanları kontrol edin; gerekirse **WSL / PX4 bul** düğmesini kullanın veya kendi dağıtım/yolunuzu girin. Bu bilgiler yalnız kendi bilgisayarınızdaki Qt ayarlarında saklanır.
4. **Araç profilleri / çoklu drone** sekmesinde ilk deneme için üç aracı ve varsayılan sıfır yük/CG/GPS gürültüsü değerlerini bırakın.
5. **Simülasyonu başlat** düğmesine basın. SYS 1–3 için taze telemetri ve `YERDE / DISARMED` durumu geldikten sonra **3B GÖREV SAHNESİ** veya **TEST LABORATUVARI** sekmesine geçin.
6. İlk deney olarak **Havada sabit kalma (hover)** hazır senaryosunu seçin, sınırları inceleyin ve **Testi çalıştır** düğmesine basın. Otomatik deney kendi ARM/kalkış/iniş adımlarını yönetir.

Detaylı Türkçe kullanım için [QUICK_START_TR.md](QUICK_START_TR.md), WSL/Gazebo notları için [SITL_SETUP.md](SITL_SETUP.md).

## Mimari

| Modül | Görev |
|---|---|
| `AeroCore_Telemetry` | MAVLink ayrıştırma ve protokol yardımcıları |
| `AeroCore_Core` | Senaryo ve sonuç veri modelleri |
| `AeroCore_Simulation` | WSL/PX4 oturumu, araç bağlantıları, deney yürütme |
| `AeroCore_UI` | Qt panelleri, harita, 3B sahne, rapor ve tekrar oynatma |

WSL/PX4 algılama ilk açılışta kullanıcı ev dizinini sınırlı derinlikte tarar. PX4 başka bir dağıtımda veya konumdaysa alanları elle düzenleyebilirsiniz. Kullanıcıya ait `build/`, Qt Creator ayarları ve uçuş kayıtları `.gitignore` ile depo dışında tutulur.

## Derleme ve test

Qt 6 ve CMake kurulu bir ortamda:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Qt olmadan yalnız protokol, matematik ve Python testleri için:

```sh
cmake -S . -B build-core -DAEROCORE_CORE_ONLY=ON
cmake --build build-core --parallel
ctest --test-dir build-core --output-on-failure
```

Geliştirme ortamında Linux/Qt 6.4 derlemesi ve testler çalıştırıldı; Windows/Qt 6.12 üzerinde uygulama kullanıcı tarafından açılıp çalıştırıldı. Bu, başka bilgisayarların PX4/Gazebo kurulumlarının doğrulandığı anlamına gelmez.

## Ölçümlerin sınırları

- 3B sahne **telemetriden çizilen bir referans görünümüdür**; Gazebo'nun grafik çıktısı veya kamera görüntüsü değildir. Varsayılan x500 modeli uygulamaya otomatik kamera videosu sağlamaz.
- Simülasyonun batarya tüketimi kalibre edilmiş fiziksel enerji modeli değildir. Yük ve rüzgâr benzetimleri de gerçek araç sertifikasyonu için yeterli değildir.
- Sensör arızası ve failsafe davranışı kullanılan PX4 derlemesine bağlıdır; komut onayı tek başına etki kanıtı sayılmaz. Ölçüm kanıtı eksikse sonuç `INCONCLUSIVE` olabilir.
- Oturum kaydı uygulamanın telemetri örnekleridir; PX4 ULog veya Gazebo video dosyası yerine geçmez. Araç karşılaştırması eşzamanlı sürü uçuşu değil, sırayla yapılan deneydir.

 `LICENSE` dosyası seçmelidir.
