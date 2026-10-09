# AeroCore 0.7.2 — 3B uçuş stüdyosu ve çoklu drone deney laboratuvarı

Bu paket kaynak koddur. Önceki EXE yeni özellikleri içermez: yeni klasörden derleyin.

## İlk açılış: terminal kullanmadan üç drone

1. ZIP'i **yeni bir klasöre** çıkarın. Eski `build` klasörünü kopyalamayın.
2. Qt Creator → **Open Project** → `AeroCore_Simulator/CMakeLists.txt`.
3. Qt 6 / MinGW 64-bit kitinizi seçin. **Ctrl+B** ile derleyip **Ctrl+R** ile çalıştırın.
4. **Araç profilleri / çoklu drone** sekmesinde drone sayısını 3 bırakın.
5. İlk denemede üç aracın **yük, CG X/Y/Z ve GPS gürültüsü değerlerini 0** bırakın.
6. İlk açılışta uygulama WSL dağıtımını ve onun Linux ev klasöründeki PX4 kaynak ağacını arar. Üstteki iki alanın doğru dolduğunu kontrol edin. Bulunamazsa **WSL / PX4 bul** düğmesini kullanın veya sizin dağıtım adınızı ve WSL içindeki PX4 klasör yolunuzu elle girin. `Ubuntu-24.04` ve `~/PX4-Autopilot` yalnız örnektir; herkeste aynı olmaz.
7. **Simülasyonu başlat** düğmesine bir kez basın. Gazebo dünya servisi hazır olduktan sonra üç PX4 oturumu açılır.
8. Telemetri tablosunda **SYS 1, SYS 2, SYS 3** ve taze **YERDE / DISARMED** bilgilerini bekleyin.
9. Uçuş konsolunda SYS seçerek seçili aracın ARM, kalkış ve inişini yönetebilirsiniz. Haritada üç aracın izi farklı renktedir. Profil sekmesinde üç ayrı telemetri tabanlı drone görünümü vardır.

## 3B sahne ve mavi/lacivert arayüz

**3B GÖREV SAHNESİ** sekmesi, gerçek MAVLink telemetrisindeki konum, irtifa ve tutumdan perspektif bir sahne oluşturur. Pembe SYS 1, kırmızı SYS 2 ve sarı SYS 3 aynı sahnede izleri ve yükseklikleriyle görünür. Bir drone'a tıklayarak onu seçin, fareyle sürükleyerek sahneyi döndürün, tekerlekle yakınlaşıp uzaklaşın. Shift+sürükle ile yerel düzlemi kaydırın; **Seçili drone'u izle** ve **Görünümü sıfırla** düğmeleri konumu geri getirir. Harita, karşılaştırma grafiği, tekrar oynatma ve sekmeler aynı araç renklerini kullanır. Rota da sahnede işaretlenir.

Arayüz zemini açık mavi, başlık ve kontroller laciverttir; pembe/kırmızı/sarı yalnız araçları birbirinden ayırır. Qt Creator 6.12'de 3B sahnenin UI testinde kullanılan `findChild` için gerekli Qt meta nesne tanımı eklendi.

Sahne yerel koordinatları görselleştiren bir 3B **referans grididir**; Gazebo arazi görüntüsü ya da kameradan alınmış piksel değildir. Telemetri gelmeden uçuş gösterilmez. Kayıt oynatma sekmesindeki **3B SAHNE** aynı oturum verisinden yeniden oluşturulur.

**Video kaynağı:** Sahnenin yanındaki **Video dosyası seç** ile MP4/MOV/MKV açabilirsiniz. Gerçek bir RTSP veya HTTP akışı varsa adresini girip **RTSP / HTTP aç** seçin. Her SYS için ayrı adres hatırlanır; SYS değişince kaynak da değişir. Kamera, ancak gerçekten çözümlenmiş video karesi gelince “GERÇEK VİDEO ÇERÇEVESİ” gösterir. Gazebo x500 bu paketle otomatik bir video sunmaz; simülatör kamera akışını ayrıca kurmanız veya bir video seçmeniz gerekir. Video oynatımı için Qt kurulumunuzda **Qt Multimedia** ve **Qt Multimedia Widgets** bileşenleri olmalıdır. Bu bileşenler olmadan proje ve 3B sahne derlenir; video paneli neden devre dışı olduğunu gösterir.

Kayıt oynatırken **Video kaynağı** sekmesinden ilgili SYS için yerel video dosyası açın. Kayıt JSON'u video içermez; video ayrı dosyadır. **Video ofseti** dosya başlangıcı ile telemetri başlangıcının farkıdır ve elle ayarlanır. Ağ akışında kayıt zamanı eşlemesi yapılmaz.

**SYS seçmek yeni araç oluşturmaz.** Yeni araçları drone sayısı ve başlatıcı oluşturur. Sayı veya fizik profili değişikliği için araçlar yerde/DISARMED iken oturumu kapatıp yeniden başlatın.

## Araç profilleri

| Alan | Etkisi |
|---|---|
| Yük kg | x500'e sabit küresel ek yük; temel aracın tüm tasarımını değiştirmez |
| CG X/Y/Z | Ek yükün temel gövdeye göre konumu; büyük ofset aracı devirebilir |
| GPS σ | Gazebo NavSat konum sensörüne Gaussian gürültü |
| Batarya süre sn | Deney sırasında `SIM_BAT_DRAIN`; 0 = zamanla boşalma kapalı, fiziksel kapasite modeli değildir |
| Alt sınır % | Deney sırasında `SIM_BAT_MIN_PCT` |
| Batarya modeli | Raporda kaynak açıklaması; fiziksel model yüklemez |
| Kontrol parametreleri JSON | Örnek `{"MPC_XY_P":0.95}`. PX4 boot override; deneyde ayrıca readback ve geri yükleme |

Profilin fizik ve batarya alanları test oluşturucuda salt okunur gösterilir; bu değerleri **Araç profilleri** sekmesinden ayarlayın. Senaryo parametrelerine araç profilindeki parametreler uygulanır; aynı ad varsa araç profili önceliklidir. Parametrenin firmware'inizde bulunması ve desteklenen türde olması gerekir. Geçersiz readback deneyin başarılı sayılmasını engeller.

**Batarya testi** için profilin süre değerini örneğin 45 sn ayarlayın; 0 bırakırsanız düşük batarya davranışı tetiklenmez. Kalibre edilmiş enerji modeli olmadığından `telemetryEnergyWh` tasarımın gerçek uçuş dayanıklılığı olarak yorumlanmaz.

## Aynı senaryoda üç aracı karşılaştırma

1. Test laboratuvarından Hover veya Rota senaryosunu seçin. Süre, irtifa, hız, rota, olaylar ve geçme/kalma sınırlarını belirleyin.
2. Üç araç da bağlı, yerde ve DISARMED olsun. Bekleyen manuel komut bulunmasın.
3. Profil sekmesindeki **Araçları aynı senaryoda karşılaştır** düğmesine basın.
4. Motor sırayla SYS 1 → SYS 2 → SYS 3 üzerinde koşulları hazırlar, ARM/kalkış yapar, ölçer, indirir ve parametreleri geri yükler.
5. Uçuş/temizlik doğrulanamazsa sonuç `INCONCLUSIVE` olur ve sonraki araç başlatılmaz. Eşik ihlali `FAIL` olsa bile temiz iniş/geri yükleme doğrulanmışsa sıradaki araç test edilebilir.
6. Bitince ilgili sonuçlar otomatik seçilir. Planlanan/gerçekleşen **yerel** rota, hata grafikleri ve araç kimlikleri birlikte görünür. **HTML rapor** düğmesi seçili sonuçları tek dosyada dışa aktarır.

Rota her aracın kendi deney başlangıcına göre Doğu/Kuzey metre cinsindedir: aynı **göreli rota** uygulanır; araçlar farklı başlangıçlarda olduğundan mutlak enlem/boylam rotaları aynı değildir. Başlangıç modelleri 8 m aralıklıdır. Rüzgâr aynı dünyanın ortak koşuludur. Diğer araçlar yerde tutulur; testler eşzamanlı uçuş veya sürü formasyon kontrolü değildir. Deneyler Gazebo RNG'yi sıfırlamaz; bilimsel tekrarlanabilirlik için başlangıç ve ortam farklılıklarını raporla birlikte değerlendirin.

## Otomatik sonuçlar ve oturum kaydı

- Her deney bitince uygulama veri klasöründeki `experiments/` altında UUID adlı **JSON + HTML** raporu oluşur. HTML tarayıcıdan açılabilir/yazdırılabilir.
- JSON: araç kimliği, profil/firmware SHA256, koşullar, eşikler, örnekler, olaylar ve metrikler. CSV ve çoklu HTML dışa aktarımı sonuç panelindedir.
- Oturum başlatılınca tüm araçların pozisyon/açı/durum bilgileri yaklaşık **10 Hz** kaydedilir. Başlatıcı ve deney olayları da kayda eklenir.
- **Kaydı dosyaya tamamla** düğmesi kaydı yazar ve dosya yolunu panel günlüğünde gösterir. Oturumu kapatınca veya uygulamadan çıkınca da kayıt tamamlanır.
- Kayıt yeri: Windows'ta genellikle `%APPDATA%/AeroCore/AeroCoreSimulator/recordings/`. Kesin yol panel kaydında görünür. Deney raporları komşu `experiments/` klasöründedir.
- Bellek sınırı: 18000 görüntüleme örneği, en fazla 10000 olay/2 MiB olay metni. Sınır dolunca mevcut kayıt yazılıp tamamlanır. Bunlar telemetri görüntüleme örnekleridir; Gazebo video kaydı veya PX4 ULog yerine geçmez.

## Kayıt oynatma

**Kayıt oynatma → Oturum kaydı aç** ile kaydedilmiş JSON'u seçin. Oynat/duraklat, 0.5×/1×/2×/4× hız ve zaman çubuğunu kullanın. SYS seçerek araç görünümünü değiştirin; haritada tüm araçların izleri vardır. Eski verinin yaşı korunur; kesinti anları canlı veri gibi gösterilmez.

Test sonuçlarından **Seçili deney kaydını oynat** seçeneği yalnız ölçüm aralığını gösterir. Eski sonuçlarda ARM/iniş durumları kaydedilmediği için bu görünümde drone modelinin uçuş durumu varsayımsaldır ve açıklama ekranda belirtilir. Tam oturum kaydı gerçek alınan ARM/iniş durumlarını içerir. Kayıt sekmesi ayrı veri modelini kullanır ve MAVLink komutu göndermez.

## Doğrulama ve sınırlar

C++20 / Qt 6.4.2 Linux derlemesi ve CTest otomatik testleri doğrulandı. Üç PTY/PX4 sürecinin model/instance/port ayrımı, erken Gazebo hatasında PX4 açılmaması, komut yönlendirmesi, bağlantı arızası izolasyonu, kayıt JSON'u, üç araç renklerinin sahnede çizimi ve UI bağları test edildi. Bu testler için sahte PX4/Gazebo süreçleri kullanılır. Gerçek Windows/WSL2/PX4/Gazebo oturum testi kullanıcı bilgisayarında yapılmalıdır; bu pakette denenmiş gibi gösterilmez.

WSL2 NAT IPv4 gereklidir; mirrored-network özel olarak desteklenmez. UDP 14560 başka uygulamada açıksa başlatma durur. PX4 tarafında 14561/14562/14563 kullanılır. Standart simülatör kaynaklarının `build/px4_sitl_default/etc` veya `rootfs/etc` altında olması gerekir. WSLg/Gazebo ve PX4 zaten kurulu olmalıdır. GTest, OpenCV, SerialPort, MAVSDK, TensorRT bağımlılığı gerekmez.

PX4 sürümünde farklı parametreler varsa readback hata verir. `Arming denied` varsa zorla ARM yapılmaz; paneldeki SYS ile etiketli preflight açıklamasını inceleyin. Çalışan başka PX4 oturumu varsa başlatıcı bunu algılar ve yeni oturumu açmaz; başka WSL/Gazebo süreçlerini topluca öldürmez.
