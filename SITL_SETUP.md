# AeroCore 0.7.2

Panelden çoklu araç kurulumu için [QUICK_START_TR.md](QUICK_START_TR.md) belgesini izleyin. Bu sürüm ortak UDP 14560 ve araç başına 14561–14563 kullanır. Eski yönergeleri yeni başlatıcıyla birlikte elle çalıştırmayın.

## Önceki tek araç kurulum referansı

# AeroCore 0.5.0 — Windows / WSL2 hazırlığı

Bu sürüm, daha önce çalışan PX4 + Gazebo ortamınızın üzerine kurulur. Qt tarafında yalnız Core, Gui, Widgets ve Network gerekir.

1. Önceden terminalden başlattığınız PX4 oturumu varsa bir kez kapatın.
2. ZIP'i yeni klasöre çıkartıp `AeroCore_Simulator/CMakeLists.txt` dosyasını Qt Creator ile açın. Eski `build` dizinini kopyalamayın.
3. Derleyip çalıştırın. İlk açılışta WSL dağıtımı ve ev klasöründeki PX4 kaynağı aranır. `Ubuntu-24.04` ve `~/PX4-Autopilot` örnek değerlerdir; farklı kurulumda **WSL / PX4 bul** veya elle giriş kullanın. Önceki sürümde kaydedilen yol korunur.
4. Test laboratuvarından senaryo seçin; ardından üstteki **Simülasyonu başlat** düğmesine basın.
5. Başlatıcı özel test modelini geçici klasörde hazırlar; PX4 SITL derlemesini yapar; özel Gazebo dünyasını başlatır ve PX4'ü o modele bağlar. Ayrı bir Gazebo transport partition kullanır; başka bir Gazebo dünyasına bağlanmaz.
6. Canlı konum ve YERDE / DISARMED geldikten sonra **Testi çalıştır** veya **Toplu deneyi çalıştır** düğmesine basın. Otomatik test için ana konsoldan ARM yapmanız gerekmez.
7. Yük / GPS gürültüsü profilini değiştirdiyseniz yerde simülasyonu kapatın ve yeniden başlatın.

## Desteklenen ortam

- Windows + WSL2 **NAT** ağı, WSLg, Ubuntu 24.04.
- WSL içinde Python 3 standart kütüphanesi, make, gz, pgrep, hostname ve ip.
- PX4 kaynak ağacı: `Tools/simulation/gz/models/x500`, `x500_base`, `worlds/default.sdf` ve çalışan `px4_sitl_default` derlemesi.
- Gazebo Harmonic (gz-sim8) WindEffects, standart PX4 sensör sistem eklentileri ve model kaynakları.
- Otomatik Linux yerel başlatma / WSL mirrored networking bu sürümde desteklenmez.

## Ağ

Panel WSL IPv4 ve NAT ağ geçidini otomatik bulur. PX4 tarafında UDP 14561, Windows tarafında 14560 kullanır. MAVSDK'nin 14540 portunu değiştirmez. Heartbeat, attitude, position, battery/status ve landed state yayınları açılır. Başka uygulama 14560 portunu kullanıyorsa panel bağlantıyı başlatmaz.

Bir GCS bağlantı kopması deneyi yapılacaksa başka bir yer istasyonunun PX4'e heartbeat göndermediğini kontrol edin; başka heartbeat, PX4 failsafe tepkisini etkileyebilir.

## Hata olduğunda

Konsoldaki **Kayıtları dışa aktar** düğmesiyle süreç kaydını alın. `AERO_ERROR`, `AERO_PROFILE`, `AERO_FIRMWARE`, PX4 STATUSTEXT ve komut sonuçları tanıda kullanılır. `invalid mode` hatası için önceki sürümdeki düzeltme korunur: `mavlink -m custom` ve yayın hızları tek tek ayarlanır.

Sensör enjeksiyonu desteklenmezse test sonucu başarı göstermez. Rüzgâr abonesi bulunamazsa olay uygulanmış gibi gösterilmez. Başlatıcı sadece kendi oluşturduğu süreç grubunu kapatır; diğer WSL oturumlarını sonlandırmaz.

Test kapsamı, veri geçerliliği ve fiziksel model sınırları için [README.md](README.md) dosyasına bakın.
