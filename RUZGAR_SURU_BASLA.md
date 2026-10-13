# Rüzgar Hamlesinde Dinamik Sürü

## Kullanım

IHA Maker sekmesinde drone özelliklerini gir, görev listesinden Rüzgar Hamlesinde Dinamik Sürü seç, IHA Maker görevini başlat düğmesine bas. Üç aynı özel drone üretilir. Önce sabit V-formasyonu referansı, ardından yerde olduğu doğrulanmış araçlarla adaptif koşu otomatik başlar. Mevcut üç yön ve görev devri seçenekleri korunur. Referans PDF görüntüleme düğmesi kaldırıldı; sonuç PDF'si laboratuvarın Sonuçlar bölümündedir.

## Görev

Üç araç ayrı geçiş irtifalarında kalkar; taze telemetriyle 5 m / 30 derece V hedeflerine ulaşmaları beklenir. Lider SYS1, formasyon irtifası 8 m'dir. Formasyon doğrulandıktan sonra 90 saniyelik ilerleme ölçümü başlar; hedef doğu yönünde 40 m'dir. 30–50 saniye arasında 3 m/s, 90 derece ortak Gazebo rüzgarı kullanılır. Bu lokal türbülans değil, dünya genelinde ortak rüzgardır.

Adaptif koşuda liderin canlı tahmini akımı başlangıç tahmini akım ortalamasının 1.4 katına iki saniye boyunca ulaşırsa kontrol değişimi başlar. Önce SYS1 2 m alçalır; bu hedef doğrulanınca SYS2 öne, SYS1 arkaya ve 6 m irtifaya geçer. Değişim ancak gerçek konum/irtifa telemetrisinden doğrulanınca kayıt edilir. Rüzgar bitince önce irtifa ayrımı, ardından ilk lider düzenine dönüş hedeflenir. Görev sonunda kendi başlangıç konumlarına dönüp iniş/DISARM doğrulanır. Zaman aşımı veya eksik onay başarılı sonuç yapılmaz.

## Bilimsel sınırlar

Bu sürüm formasyon kontrol prototipidir. Rotor iz etkileşimi, kuş sürüsü benzeri aerodinamik shielding, lidere özel rüzgar bölgesi, gerçek motor akımı ve kalan menzil modeli yoktur. İrtifayı 2 m azaltmanın enerji tasarrufu sağladığı varsayılmaz. Akım, girilen askı akımı ve telemetrideki yatış açılarından tahmin edilir. Yüzde 40 artış üretilmez; eşik gerçekleşmezse lider değişimi tetiklenmez ve adaptasyon INCONCLUSIVE olarak raporlanır. Yüzde 18 tasarruf sabit değildir: iki geçerli koşunun tahmini toplam mAh farkı kullanılır; artan tüketim negatif tasarruf olarak çıkar. Bunlar gerçek batarya/akım ölçümü veya aerodinamik fayda kanıtı değildir.

## Sonuç PDF'si

Laboratuvar Sonuçlar listesinden testleri seç, Deney sonuç PDF'si düğmesine bas. Rapor görev durumu PASS/FAIL/INCONCLUSIVE, sebep, formasyon hatası, lider değişiminin doğrulanması, tahmini tüketim ve geçerliyse referans/adaptif tüketim farkını gösterir. Önceki genel deneylerin PDF'si de korunur. PDF referans olarak açılmak veya içe aktarılmak için değil, deney sonuçlarını dışa aktarmak içindir.

## Doğrulama

V geometrisi, 5 m/30 derece mesafesi, lider değişiminde 2 m irtifa farkı, eşik ve tasarruf hesap çekirdeği bağımsız C++ testinde kontrol edilir. Önceki Python model/başlatıcı testleri çalışır. Qt koordinatörü, gerçek uçuş ve PDF görünümü burada derlenip çalıştırılmamıştır. Yerel Qt/WSL testi gereklidir.
