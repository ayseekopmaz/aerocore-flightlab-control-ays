# IHA Maker deney düzeltmesi

Mevcut arayüz ve diğer özellikler korunmuştur.

- Deney doğrulayıcısı artık iha_maker_1, iha_maker_2 ve iha_maker_3 modellerini tüm mevcut deney türlerinde kabul eder.
- PX4 terminalindeki tekrarlanan pxh komut yankıları ekrana basılmaz; anlamlı durum/hata satırları korunur. Ekran günlüğü 2000 satırla sınırlıdır.
- Röle ve rüzgar sürü deneylerinde kısa telemetri gecikmelerinde yeni hareket hedefleri bekletilir; veri 3 saniye toparlanmazsa iniş başlatılır. Ölçüm boşlukları başarılı sonuç gibi sunulmaz.
- IHA Maker sekmesindeki Deney analizini PDF al düğmesi tamamlanmış sonuçları dışa aktarır. Sonuç tablosunda seçim yoksa son üç sonuç kullanılır. Rüzgar deneyinin sabit ve adaptif koşullarını birlikte raporlamak için altı sonucu seçin.

## Kullanım

Projeyi yeniden derleyin ve eski uygulamayı kapatın. IHA Maker özelliklerini girip kaydedin, IHA Maker için başlat ile üç araçlı oturumu açın. Araçlar hazır olduğunda istediğiniz deneyi başlatın. Sonuç oluşunca PDF düğmesine basıp dosya konumunu seçin.

## Kontrol

35 Python testi geçti: filo 15, model 6, başlatıcı 8, konsol filtresi 3, kısa başlatma 3. Model kimliği ve telemetri bekleme çekirdeği C++ testleri geçti. Qt derleme/arayüz testleri ve gerçek PX4/Gazebo uçuşu bu ortamda çalıştırılamadı; kullanıcı ortamında doğrulanmalıdır.

Geometri/atalet yaklaşık, elektrik tüketimi tahmindir. PDF başarı, başarısızlık veya yetersiz kanıt durumunu ve ölçüm sınırlamalarını içerir; tasarruf ya da kesintisizlik oranı sabit başarı değeri olarak yazılmaz.
