# 0.6.0 değişiklikleri

- Bir Gazebo dünyasında 1–3 x500; ayrı SYS, instance, PTY, çalışma klasörü ve UDP portu.
- Physics/SceneBroadcaster/sensör eklentileri ve açılış readiness kontrolü korunur.
- Araç profili kaydet/aç; merkezlenmiş yük/GPS/deney bataryası/kontrol parametreleri.
- Ortak haritada üç renkli iz; araç başına mini 3B durum görünümü.
- Tek senaryoda sırayla araç karşılaştırma; belirsiz temizlikte sıra durur.
- Parametre cache ve link-fault araç bazında ayrılır; gecikmeli gönderinin hedefi saklanır.
- Otomatik deney JSON + HTML; çoklu grafik/HTML, CSV dışa aktarımı.
- 10 Hz oturum kaydı; zaman arama, hız, pause ve ayrı offline replay veri modeli.
- Her deneyde SYS/ad ve ölçüm başlangıç ofseti rapora eklenir.
- Yeni testler: fleet modeller/PTY süreçleri, UDP izolasyonu, kayıt ve karşılaştırma.

Kaynaklar:
- https://docs.px4.io/main/en/sim_gazebo_gz/multi_vehicle_simulation
- https://github.com/PX4/PX4-Autopilot/blob/main/ROMFS/px4fmu_common/init.d-posix/rcS
- https://gazebosim.org/api/sim/8/server_config.html

Sınırlamalar ve kullanım için QUICK_START_TR.md. Gerçek Windows/WSL/PX4/Gazebo uçtan uca oturumu burada çalıştırılmamıştır.
