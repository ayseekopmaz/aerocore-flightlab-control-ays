# IHA Maker: üç drone başlat

1. Yeni paketi çıkar, Qt Creator'da CMakeLists.txt ile yeniden derle. Eski exe yerine bu build'i aç.
2. IHA Maker sekmesinde gönderilen PDF'nin varsayılan değerleri bulunur: 599 g, 220 mm, 5.1 inç, 3 kanat, tek pervane 4.6 g, tek motor 32 g, toplam maksimum itki 7318 g, 39699 rpm, CG 4.5/0/15.1 mm, 6S 1300 mAh, kamera açık.
3. IHA Maker başlat — 3 drone düğmesine bas. Kapalı oturumda üç araç sayısı otomatik seçilir, mevcut PX4/Gazebo başlatıcısı kullanılır; aynı donanımdan üç bağımsız iha_maker modeli üretilir. WSL/PX4 kurulum alanları önceden doğru olmalı.
4. Üç aracın yerde ve hazır telemetrisi doğrulanınca üç bağımsız görev birlikte başlatılır. Kararlı kalkıştan sonra ortak ölçüm zamanına geçilir. SYS1 doğu, SYS2 kuzey, SYS3 batı yönüne, kendi başlangıçlarına göre ayrı rotalarda gider. İrtifalar 5/7/9 m'dir. Ortak ölçüm en az 60 saniye ve rota bitene kadar sürer; ardından iniş ve DISARM doğrulanır. Fiziksel kalkışlar birebir aynı milisaniyede garanti edilmez.
5. Görevi iptal et / iniş düğmesi üç yürütücüyü durdurup iniş temizliğine geçirir. Simülasyon kapat düğmesi uçuş sırasında önceki korumaları sürdürür.
6. Sonuçlar mevcut laboratuvar sonuç listesine üç ayrı test olarak eklenir. PDF/JSON dışa aktarımı korunur.

Mevcut üç araçlı IHA oturumu varsa düğme görevleri başlatır. Tek araç veya farklı model oturumu varsa kapatıp yeniden başlatman istenir; çalışan oturum sessizce değiştirilmez. Bir araç testi tamamlayamazsa kalan görevler de iptal/inişe geçer; hata başarılı sonuç gibi raporlanmaz.

## Görünüm

3B telemetri sahnesi IHA Maker açılışında yeşil arazi, düşük poligonlu ağaçlar, kırmızı bataryalı quad çizimi ve canlı yatış göstergesiyle referansa yaklaştırılır. Bu QPainter perspektif sahnesidir, IHA Maker'ın ekranı veya model dosyasının birebir aktarımı değildir. Ağaçlar dekoratiftir; Gazebo çarpışma engeli değildir. Gerçek Gazebo özel modelinde kırmızı batarya ve siyah kayış görseli eklenir. Diğer arayüz/Designer dosyaları korunur.

Kapsam: aynı PDF tasarımıyla üç farklı yön rotası. Araç başına farklı donanım seçimi, batarya rölesi, otomatik görev devri, yeni kamera aktarımı veya başka yeni özellik eklenmemiştir. Önceki enerji tahmini aynı sınırlamalarla devam eder.

## Doğrulama

Python başlatıcı/model testleri ve bağımsız C++ enerji hesabı kontrol edilir. Qt geliştirme kütüphaneleri ve Gazebo ortamı bu yürütücüde yoktur; yeni Qt görev koordinatörü, görsel görünüm ve gerçek üçlü uçuş burada çalıştırılamamıştır. Yerel derleme ve SITL denemesi gerekir.
