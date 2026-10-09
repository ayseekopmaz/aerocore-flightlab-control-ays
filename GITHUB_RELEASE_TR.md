# AeroCore'u GitHub'a koymadan önce

Bu depo **kaynak kodu** içerir. Bir Windows EXE veya PX4/Gazebo kurulum paketi değildir.

## Başka bir kullanıcının bilgisayarında

1. Windows 10/11, WSL2 içinde Ubuntu ve PX4-Autopilot kaynakları ile Gazebo kurulmalıdır. WSLg/Gazebo ve PX4 sürümleri de uyumlu olmalıdır. AeroCore bunları otomatik **kurmaz**.
2. Qt 6.4+ Core, Gui, Widgets, Network; C++20 derleyici; CMake, Ninja gerekir. Video dosyası/RTSP için ayrıca Qt Multimedia + Multimedia Widgets gerekir. Diğer modüller bu eklenti olmadan derlenir.
3. Qt Creator'da kökteki `CMakeLists.txt` açılır, Qt 6 kiti seçilip Build → Run yapılır.
4. İlk açılışta uygulama kurulu WSL dağıtımını listeler ve seçtiği dağıtımın **Linux ev klasöründe**, en fazla dört alt dizin derinliğinde, PX4 kaynaklarını arar. Bulunursa üstteki alanlar dolar. Yoksa **WSL / PX4 bul** ile yeniden tarayın veya iki alanı elle girin. Arama yalnız kaynak klasörlerini okur; kurulum veya klonlama yapmaz.
5. Birden çok WSL dağıtımı varsa ilk Docker olmayan dağıtımı önerir. PX4 başka dağıtımda veya farklı konumdaysa dağıtım/yol alanlarını elle düzeltin. Seçim yalnız o bilgisayarın Qt kullanıcı ayarlarında saklanır, GitHub'a yüklenmez.
6. Simülasyon başlatma yalnız Windows+WSL2/NAT yolunda çalışır; 3B/arayüz Linux'ta derlenebilir ama doğrudan Linux PX4 otomatik başlatma bu sürümde yoktur.

## Git deposunu hazırlama

- `build/`, Qt Creator `.user` dosyaları, telemetri/log kayıtları, uçuş verileri ve gizli ayarlar `.gitignore` ile hariç tutulur. `git status` ile gerçekten neyin yüklenmek üzere olduğunu kontrol edin.
- Açık kaynak lisansını proje sahibi seçip ayrıca bir `LICENSE` dosyası eklemelidir. Lisans seçmeden başkalarına kullanım izni verdiğiniz varsayılmamalıdır.
- Belgelerdeki `Ubuntu-24.04`, kullanıcı adı ve eski Windows yollarını sabit makine gereksinimi olarak sunmayın. Bu paket ortamı tarar, kullanıcı gerekirse değiştirir.
- Gerçek WSL/Gazebo/PX4 uçuşu her Windows/kit bileşiminde ayrıca sınanmalıdır. Linux testleri PX4/Gazebo süreçlerini çoğunlukla taklit eder.

GitHub'da yeni **boş** depo açtıktan sonra bu klasörde:

```bash
git init
git add .
git status
git commit -m "Initial AeroCore simulator source"
git branch -M main
git remote add origin https://github.com/KULLANICI/REPO.git
git push -u origin main
```

`KULLANICI/REPO` bölümünü kendi depo adresinizle değiştirin. GitHub'da daha önce README veya lisans oluşturarak dolu bir depo açtıysanız önce dalları birleştirmek gerekir; komutları olduğu gibi çalıştırmayın.
