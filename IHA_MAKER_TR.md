# IHA Maker özel Gazebo modeli

1. IHA Maker sekmesindeki model seçeneği varsayılan olarak işaretlidir.
2. Yeni PDF V2 (599 g / 220 mm / 5.1 inç / 6S) profili uygulama kaynağından yüklenir. Eski V1 profili profiles/iha_maker_v1.json içinde korunur. Tasarım bağlantısı IHA Maker'ı tarayıcıda açar. Atölyeyi parça seçimi ve ilk hesaplarda kullanmaya devam edin.
3. Tek drone için Araç profilleri sekmesindeki araç sayısını 1 yapın. 1–3 araç korunur; her araç aynı IHA donanımını, kendi ek yük/GPS ayarını kullanır.
4. Normal Başlat düğmesini kullanın. Başlatıcı, iha_maker_1 (varsa _2, _3) isimli bağımsız Gazebo modelleri üretir. Profil hata verirse x500'e düşmez.
5. PDF V1'de toplam 73 g itki / 615 g kalkış ağırlığı: kalkış beklenmez. Motorları yapay biçimde güçlendirmedik. Atölyede donanımı düzeltin, yeni PDF'ye göre gram/mm/inç/itki/devir alanlarını güncelleyin, profili kaydedip oturumu yeniden başlatın. PDF otomatik ayrıştırılmaz; link otomatik senkronize olmaz.

## Gerçekte uygulanan fizik

Kalkış kütlesi, aks açıklığı, CG (ileri/sağ/yukarı mm -> Gazebo ileri/sol/yukarı m), pervane çapı, maksimum devir ve maksimum toplam itki modele aktarılır. T=k*omega^2 katsayısı PDF maksimum itki ve devrinden türetilir. Her motor fizik plugin'i, bağlantısı ve propeller görseli ayrıdır. PX4 rotor konumu, yön/tork işareti ve aktüatör hız aralığı özel modelle eşleştirilir. Ek yük ve GPS gürültüsü laboratuvar profillerinden uygulanır. Model/kayıt özeti fizik değerlerini, donanımı, varsayımları ve SHA256 içerir.

## Yaklaşım sınırları

Bu, ekran görüntüsü/PDF'den oluşturulmuş prosedürel bir modeldir; gerçek iFlight CAD aktarımı değildir. Gövde/kol/batarya/motor/kamera geometrileri yaklaşık kutulardır. Atalet merkezi kutu+dört motor nokta kütlesi yaklaşımıdır. Motor yanıtı ve tork katsayısı simulation alanında açık varsayımlardır. Hızlı uçuşta aerodinamik sürükleme kalibre edilmemiştir. Batarya PX4 sentetik tüketimiyle kalır; PDF elektriksel değerleri batarya/ESC ısıl veya elektriksel testine dönüşmez. SITL fiziksel SpeedyBee kartını veya firmware'ini emüle etmez.

PX4 kurulumundaki IMU, GPS, barometre ve varsa manyetometre sensör tanımları uyumluluk şablonu olarak okunur. x500_base/model.sdf bu nedenle kaynakta gereklidir; geometrisi, kütlesi, ataleti, motor plugin'leri kopyalanmaz. PX4 4001 quad başlangıç yapılandırması kullanılır; özel geometrinin aktüatör/rotor parametreleri boot overrides ile değiştirilir. PX4 kaynakları değiştirilmez; her instance ayrı geçici rootfs kullanır.

## Kamera

Özel modelde 640x480, 30Hz ileri bakan simülasyon kamerası vardır. Gazebo GUI Image Display eklentisinde /iha_maker/iha_maker_1/camera konusunu seçin; konu akışı hazır olduğunda görüntü görülür. Gazebo görüntüsü Qt RTSP oynatıcısına otomatik bağlanmaz. Mevcut CameraPanel video/RTSP/HTTP özellikleri korunur. Kamera sensörü DJI O3 video sistemi emülasyonu değildir.

## Kontrol

IHA Maker seçeneğini kaldırmak önceki x500 iş akışını geri getirir. Varsayılan yeni iş akışı özel IHA modelidir. Değişiklikler oturum başlatılırken snapshot olarak alınır; uçuş esnasındaki JSON değişiklikleri ancak sonraki başlatmada uygulanır.


## Güncel V2 raporu

IHA Maker artık Uçar mı? Evet sonucu verir. Profilde iFlight Nazgul5 V3, 4x2207 1950KV motor, Gemfan Hurricane 51466 5.1 inç pervane, 55A ESC, F405 V4, 6S 1300mAh batarya, DJI O3 ve tek RP1 alıcı vardır. 599 g toplam kütle, 220 mm aks açıklığı, CG 4.5/0/15.1 mm, 7318 g maksimum toplam itki ve 39699 rpm maksimum devir model girdileridir. 383 saniye uçuş süresi atölye tahminidir; Gazebo test sonucu değildir.

Basit Gazebo T=k*omega^2 yaklaşımı yalnızca rapordaki maksimum itki/devir noktasına uydurulur. Atölyenin yüzde 20 askı gazı ve 8383 rpm askı devri aynı eğriyi garanti etmez; simülasyon askı noktası ayrıca ölçülmelidir. 12.2:1 itki/ağırlık oranı üretici test eğrisi ile doğrulanmamıştır.

V2 parça listesinde GPS yoktur. Konum kontrollü PX4 SITL testleri için ideal NavSat sensörü modele eklenmiştir. Bu, gerçek tasarımda GPS bulunduğu anlamına gelmez. Görev testleri için fiziksel konum kestirim çözümü ayrıca tasarlanmalıdır.


## Sayısal özellik formu

IHA Maker sekmesi marka seçimi veya JSON editörü içermez. Gram, mm, inç, rpm, S, mAh ve A birimleriyle alanlar vardır. Profil kaydet/yükle JSON biçimini arka planda kullanır. Kamera var seçeneği sensör/görseli gerçekten açar veya kapatır; çözünürlük ve kare hızı sensöre uygulanır.

Uygulanan girdiler: toplam kalkış kütlesi, aks açıklığı, pervane çapı, pervane/motor kütlesi, ağırlık merkezi, gövde boyutları, toplam maksimum itki, maksimum devir, motor yanıt süreleri ve tork/itki katsayısı. Pervane kanat sayısı görseli değiştirir. Pervane hatvesi kaydedilir; gerçek itki eğrisi bağımsız olarak girilir. Motor KV, batarya S/mAh ve ESC A bilgileri elektriksel/termal simülasyon yapmaz. Toplam kalkış ağırlığı kamerayı da içerir; kamerayı kapatınca toplam kütleyi yeni atölye raporuna göre ayrıca güncelleyin.

Kütle/çap tutarsızlığı önceden gösterilir. Kamera seçenekleri dışında sensörler konum kontrollü SITL için uyumluluk şablonudur. PDF açma yalnızca görüntüleme yapar; otomatik PDF içe aktarma yoktur.


## Windows başlatma düzeltmesi ve 60 saniye testi

Dosya adı veya uzantısı çok uzun hatası uzun Python kodunun Windows komut satırında taşınmasından kaynaklanıyordu. Kısa bootstrap komutu kullanılır; kod/profil stdin üzerinden aktarılır, WSL içinde geçici dosyalara yazılıp çalıştırılır. Durdurma hattı korunur.

Qt Creator'da bu paketin CMakeLists.txt dosyasını açıp yeniden derleyin. IHA Maker özellikleri açık, araç sayısı 1 olsun. Simülasyonu başlatın. Uçuş laboratuvarında senaryo yükleme ile scenarios/iha_hover_60s.json dosyasını açın. Bu senaryo havada kalma ölçümünü 60 saniye tutar; kalkış/iniş toplam süreye eklenir. Canlı oturum ve konum geldiğinde laboratuvar deneyi çalıştırılır.

Sonuç JSON/CSV ve mevcut HTML karşılaştırması uçuş verilerinden oluşur. Bu sürümdeki batarya yüzdesi PX4 sentetik batarya verisidir; 1300 mAh/9.8 A fiziksel akım entegrasyonunu temsil etmez. PDF'deki 6:23 uçuş süresi veya yüzde 12.6/dakika tahminiyle ölçülmüş tüketim gibi karşılaştırmayın. Elektriksel tüketim modeli henüz bağlanmamıştır.

Gerçek simülasyon çalışması bu ortamda koşulmadı. Uçuş sonrası sonuç JSON'u ile ayrı AeroCore analiz PDF'si hazırlanabilir: kullanılan model/varsayımlar, tamamlanan süre, irtifa/konum hataları, yatış açıları ve atölye tahminlerinin karşılaştırması. Test yapılmadan PASS/FAIL veya batarya yeterlilik sonucu üretilmez.
