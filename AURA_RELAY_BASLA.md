# Aura-Relay: IHA Maker tasarımından nöbet değişimi

Mevcut genel özellikler ve üç ayrı yön görevi korunmuştur. IHA Maker sekmesindeki görev listesinden Aura-Relay nöbet değişimi seçilir, IHA Maker başlat — 3 drone düğmesine basılır. Aynı PDF özellikleriyle üç ayrı Gazebo/PX4 aracı başlatılır. Önceden çalışan üç IHA Maker aracı yerdeyse mevcut oturum kullanılır; uçan araçlara yeni görev sessizce yüklenmez.

## Akış

Drone 1 kalkar, ortak devriye alanına gider ve dört noktalı rotayı döngü halinde yürütür. Drone 2 %100 tahmini SOC ile yerde yedektir. Drone 3 %95 model başlangıcıyla yerde ideal şarjdadır. SOC oranları gerçek batarya ölçümü veya PX4 telemetrisi değildir.

30. saniyede Gazebo dünyasına 3 m/s, 90 derece rüzgar komutu gönderilir. Komutun başarılı yanıtı kayda alınır. Bu dünya genelinde ortak rüzgardır, yalnız Drone 1'e özel değildir. Gerçekçi 9.8 A / 1300 mAh varsayımında pil 30 s içinde %25 olmaz; böyle bir sonuç zorlanmaz.

Aktif aracın canlı tahmini SOC'si %25'e düşünce %99 ve üzeri yerde hazır yedek aranır. Yedek kalkar ve ortak devriye merkezine gider. Taze telemetriyle hedefe 3 m yakınlık, irtifa uygunluğu ve 2 saniye kalma doğrulanmadan aktif araç dönmez. Devir doğrulanınca eski araç RTL komutu alır. RTL/iniş ve DISARM doğrulandıktan sonra yerde şarj modeline geçer. Hazır yedek yoksa %10 kritik eşikte test durdurulur ve iniş istenir; kesintisizlik uğruna kritik enerji varsayımı aşılmaz.

Varsayılan deney 900 s (15 dk); iniş/temizlik ek süre alır. Şarj modeli 1.3 A sabit akımdır, gerçek LiPo CC/CV şarjı değildir. Üç bataryanın bu modelle sonsuza dek devriye sürdüreceği iddia edilmez. Testte bir veya birkaç devir görülebilir; başarısız/devirsiz test PASS yapılmaz.

## Canlı görünüm

IHA Maker sekmesinde üç aracın rolü, tahmini SOC, tahmini akımı, sabit varsayılan gerilimi ve SOC eğrileri gösterilir. Değerler live hover-current/tilt estimate olarak etiketlidir. Motor akımı MAVLink'ten ölçülmez; tahmini gerilim batarya azalırken fiziksel gerilim düşüşüne dönüşmez. Model uçuş fiziğini, motor gücünü veya PX4'ün kendi batarya failsafe'ini değiştirmez. Kamera açık bilgisi fiziksel sensör oluşturur ama bu görevde kamera görüşü/nesne görünürlüğü hesaplanmaz.

## Kesintisizlik ölçütü ve PDF

Konumsal görev mevcudiyeti: güncel telemetri, aktif/devralan görev rolü, devriye merkezinden 25 m yarıçap ve 8 m hedef irtifaya 1.5 m yakınlık sağlayan en az bir araç bulunması. Başlangıç kalkış/varış süresinden sonra başlayan görev penceresinde kapsanan süre / izlenen süre hesaplanır. Boşluk süresi, en uzun boşluk, iki aracın aynı anda kapsaması ve devir sayısı kaydedilir. Bu gerçek saha/kamera gözlem kesintisizliği değildir; yüzde 100 sabit değildir.

Her araç için sonuçlar mevcut laboratuvar listesine eklenir. Seçili sonuçların IHA Maker karşılaştırma PDF'si düğmesi Aura-Relay bilgilerini de raporlar: konumsal mevcudiyet yüzdesi, boşluk, devir, tahmini tüketim ve ideal şarj. Olaylar SessionRecording olay kaydına, tam sonuçlar deney JSON'una yazılır. Sonuçları tekrar yükleyip PDF alabilirsin.

## İptal / hata

Görevi iptal et / iniş düğmesi tüm araçları inişe geçirir. Komut başarısızlığı, güncel olmayan uçuş telemetrisi, kritik SOC veya zaman aşımı başarısız/INCONCLUSIVE olarak kaydedilir. İniş doğrulanamazsa operatöre canlı durumu kontrol etmesi söylenir; simülasyon zorla kapatılmaz. Genel manuel komutlar görev çalışırken bloke kalır.

## Doğrulama sınırı

Portable RelayCore C++ testi tüketim/şarj hesabını, devir kapısının taze hedef teyidini ve kapsama boşluğu hesabını doğrular. Python model/başlatıcı testleri çalışır. Yeni Qt AuraRelayCoordinator, çizim ve gerçek Gazebo/PX4 görev akışı burada derlenip çalıştırılamamıştır; yerel Qt/WSL SITL doğrulaması gerekir. Sistemin termal gözlem veya 7/24 hizmet sağladığı kanıtlanmış değildir.
