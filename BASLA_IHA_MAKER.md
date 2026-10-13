# IHA Maker -> AeroCore: baştan sona

## 1. Kurulum

Paketi yeni bir klasöre çıkar. Qt Creator'da AeroCore_Simulator/CMakeLists.txt dosyasını aç. Mevcut Qt kitinle yeniden derle; eski exe'yi çalıştırma. WSL dağıtımı Ubuntu-24.04 ve PX4 klasörü kendi kurulumundaki yol olmalı. Başlat düğmesi derleme ve Gazebo/PX4 açılışını yönetir. Mevcut arayüz ve tüm önceki özellikler korunur.

## 2. Atölyede tasarla

IHA Maker'da gövde, motor, pervane, batarya ve kamerayı seç. Atölye sonuçlarını ve PDF'yi kaydet. Tasarım A ile Tasarım B için ayrı PDF sakla.

## 3. Sayısal değerleri aktar

AeroCore'un IHA Maker sekmesine PDF değerlerini elle gir: toplam kütle (g), karşı motor merkezleri (mm), pervane çapı/hatvesi (inç), kanat sayısı, tek motor/pervane kütlesi (g), toplam maksimum itki (g), maksimum pervane devri (rpm), CG ileri/sağ/yukarı (mm), batarya S/mAh, ESC A, askı akımı, yük altı gerilim ve atölye askı süresi (s). Kamera var seçeneğini işaretle. Geometri/atalet ve yanıt katsayıları PDF'de yoksa yaklaşık değerlerdir; gerçek CAD veya ölçüm değildir.

Varsayılan: 599 g, 220 mm, 5.1 inç, 3 kanat, 4.6 g/pervane, 32 g/motor, 7318 g toplam itki, 39699 rpm, CG 4.5/0/15.1 mm; 6S 1300 mAh, 55 A ESC, 9.8 A askı, 21.9 V yük altı, %80 kullanılabilir kapasite, 383 s atölye askı tahmini.

Profil kaydet ile Tasarim_A.json sakla. PDF aç düğmesi yalnız görüntüleme yapar; PDF/site otomatik aktarımı yoktur. Özellik değişiklikleri sonraki simülasyon başlatmasında uygulanır.

## 4. 60 saniye test

IHA Maker modeli seçili olsun. Araç profilleri sekmesinde araç sayısını 1 yap, ek yük ve GPS gürültüsünü ilk denemede 0 bırak. Başlat. Gazebo dünya servisinin hazır olması ve PX4 telemetrisinin gelmesini bekle. Laboratuvarda Senaryo aç ile scenarios/iha_hover_60s.json yükle. Deney başlat: sistem kalkış, kararlı havada kalma, 60 s ölçüm ve inişi yürütür. Kalkış/iniş ek zaman alır; toplam uçuş tam 60 s değildir. İkinci test scenarios/iha_route_60s.json dosyasıyla rota takibidir. Simülasyon başarısız olursa PASS uydurulmaz; kayıt hata veya INCONCLUSIVE içerir.

## 5. Analiz PDF

Sonuçlar sekmesinde bir test satırını seç. IHA Maker karşılaştırma PDF'si düğmesine bas. Rapor atölye girdilerini, uçuş ölçümlerini ve yaklaşık tüketimi ayrı gösterir. Konum/irtifa hatası, yatış ve süre uçuş telemetrisinden gelir. Akım + açı tüketim hesabı yaklaşık, test sonrası ve yalnız ölçüm bölümündedir. Motor akımı ölçülmez; kalkış/iniş, ivmelenme, ısı ve gerilim düşmesi dahil değildir. Uçuş PASS sonucu batarya yeterlilik onayı değildir. Telemetri boşlukları tüketim hesabına eklenmez.

## 6. İkinci tasarım

Atölyede değişikliği yap, Tasarım B PDF'sini al. AeroCore sayısal alanlarını güncelle, Tasarim_B.json olarak kaydet. Simülasyonu kapatıp yeniden başlat. Aynı hover/rota, yük, rüzgar, süre ve kontrol ayarlarını koru. Senaryonun versionTag alanını B olarak kaydedebilirsin. A ve B sonuçlarını JSON dışa aktar; gerektiğinde sonuç içe aktarma ile tekrar aç. Sonuçlar listesinden Ctrl ile iki satırı seçerek karşılaştırma PDF'si oluştur.

## Doğrulama sınırı

Bu paketin Python başlatıcı/model ve bağımsız enerji hesabı testleri çalıştırılmıştır. Windows/WSL'de Qt uygulamasının derlenmesi, PDF görsel çıktısı ve gerçek Gazebo uçuşu bu ortamda koşulmamıştır. Hata oluşursa olay kaydını paylaş. Batarya/ESC fizik modeli henüz uçuş dinamiğine bağlı değildir; tüketim raporu tahmindir.
