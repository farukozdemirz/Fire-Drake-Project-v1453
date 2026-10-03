# Harita yürüyüş oturumu protokolü (T-NAV-02 eğim, T-NAV-09 su, T-ENV-ARENA-02 arena, rota kaydı)

> **Durum (2026-10-03):** Karus oturumunda town → bowl, bowl'da arama, karşı kapı (Lunar War araya girdi), su (T-NAV-09 GEÇTİ) ve eğim (T-NAV-02 YAPILDI, ADR-0024) tamamlandı; sonuçlar `docs/STATUS.md` ve `docs/12` §3. Kalan: arena A turu (T-ENV-ARENA-02) ve El Morad oturumu. Bölüm 6'daki eğim tablosu ilk sürümdür; düz zeminden yaklaşılabilirlik filtresi sonradan eklendi (`tools/slope-candidates.py`).

> Okuyan: proje sahibi. Yazan: Claude (2026-10-03). Bu oturum **bot gerektirmez**: kendi (insan) karakterinle iki ırkta yürürsün, sunucu hareketlerini paket izleyiciyle kaydeder. Süre: ırk başına yaklaşık 45–60 dk. Kaynaklar: `docs/12` §13.1, `docs/15` T-NAV-02/09, `plans/F5-60`, `tools/route-extract.py`, `tools/slope-candidates.py` (ikisi de `--selftest` geçti).

## 1. Neden bu oturum, neyi çözüyor

| Test | Çözdüğü soru | Sonuç neyi belirler |
|---|---|---|
| **T-NAV-09 su** | Haritadaki 5 çukur (en derini −7,4 m) su mu, vadi mi? İstemci içine girebiliyor mu? | F5-67 (su katmanı) **yazılsın mı iptal mi** |
| **T-NAV-02 eğim** | İstemci hangi eğimi tırmanamıyor? (`maxSlope 0,625` şu an tahmin) | Botların yürünebilir saydığı eğim sınırı; kiriş guard'ı (F5-61) |
| **T-ENV-ARENA-02** | Arena koordinatları oyunda yürünebilir mi, engel/yükseklik var mı? | F1 kabulü, arena doğrulaması |
| **Rota kaydı** (senin fikrin) | İnsan town → bowl → arama → karşı kapı rotasını nasıl yürüyor? | F6-10 dolaşma/arama davranışı, F11 serbest Ronark, A* yolunu gerçeğe karşı kontrol |

## 2. Hazırlık (bir kez)

1. **Döngüyü durdur** (döngü her adımda sunucuyu kapatıp dal değiştirir):
   ```bash
   cd /mnt/c/Users/frkoz/OneDrive/Desktop/Fire-Drake-Project-v1453
   touch plans/.supervisor-stop
   touch plans/.auto-loop-stop
   tail -f plans/_logs/auto-loop.log     # "DUR:" satırını ve "auto-loop.sh bitti" satırını bekle (birkaç dk; kapanış raporu yazılır), sonra Ctrl+C
   tools/run-servers.sh status           # hepsi kapalı olmalı
   ```
2. **Temiz dal:** `git status --short` boş olmalı, sonra `git switch main`.
3. **İzleyicili sunucu:** `tools/trace-session.sh prepare` (durdurur, `--packet-trace` ile derler, başlatır; birkaç dk). Bitince "izleyicili sunucular ayakta" yazar.
4. **Giriş:** kendi normal hesabınla gir (bot hesabı değil). Botlar bu oturumda **doğurulmaz**. Ayrı iki oturum: önce Karus karakteri, sonra El Morad karakteri (arada `trace-session.sh collect` ile kayıt kesilir).

## 3. Her bölümden sonra: kayıt kes ve not al

Her bölüm bitince **hemen** `tools/trace-session.sh collect <etiket>` çalıştır (etiket 1–40 karakter, `A-Z a-z 0-9 _ -`) ve alttaki not tablosuna **saat** (duvar saati) ve ne yaptığını yaz. Kayıtlar `plans/_logs/trace/` altında toplanır; **ad içerir, paylaşma, git'e ekleme**.

Koordinat için oyundaki konum göstergesine (harita üstü x, z) bak; 6–10 m sapma sorun değil, tam konumu zaten kayıt tutar.

## 4. Oturum akışı (her ırk için aynı sırayla)

| # | Bölüm | Ne yap | Etiket örneği | Not |
|---|---|---|---|---|
| 1 | **Town → bowl** | Kendi town çıkışından bowl'a (harita merkezi ≈ (1024, 1024), yarıçap ~150 m) normal oyuncu gibi git | `karus_town_bowl` / `elmorad_town_bowl` | Yolda canavara denk gelirsen nasıl kaçındığını da yaz |
| 2 | **Bowl'da dönerek arama** | Bowl içinde dönerek düşman ara (5–10 dk); hangi yöne dönüyorsun, nerede durup bakıyorsun not al | `*_bowl_arama` | Bu, botun dolaşma davranışı için ana veri |
| 3 | **Karşı ırk kapısına** | Düşman bulunamazsa karşı ırkın kapısının önüne git, çıkmalarını bekle (5 dk) | `*_karsi_kapi` | **Güvenlik:** karşı kapının kuleleri ~26 m'de öldürüyor (T-ENV-ARENA-03: Karus kapısı (1375, 1085)); kapıya **en az 60 m** uzakta bekle, hangi mesafede durduğunu yaz. Karşı kapının koordinatını gösterge ile not et |
| 4 | **Su çukurları (T-NAV-09)** | Tablo 5'teki çukurlara sırayla git: yaklaşma noktasından **en derin noktaya doğru** yürü, içine girmeyi dene | `*_su` | Girebildin mi? Suya batma/yavaşlama/görüntü değişimi var mı? Çukur 3 ve 4 bowl'un içinde, kolay |
| 5 | **Eğim (T-NAV-02)** | Tablo 6'daki yerlerde alt noktadan üst noktaya **düz çizgide** yürü; çıkamazsan 5 sn zorla, sonra sıradaki | `*_egim` | Her satır için "çıktı / yavaşladı / çıkamadı" yaz. İlk iki bant (0,30–0,60) yeterli, vaktin varsa hepsi |
| 6 | **Arena A (T-ENV-ARENA-02)** | Arena A (1274, 890) çevresinde bir tur at; engel, yükseklik farkı, geçilemeyen yer varsa yaz | `*_arena` | Respawn → arena yürüyüşünü zaten ölçmüştük (69,8 sn / 165,1 sn), burada yalnızca yürünebilirlik |

## 5. Su çukurları (`tools/slope-candidates.py` ile aynı ızgara; F5-60 ile sayılar tutuyor: 1820+1225+247+228+219 = 3739 hücre)

| # | Hücre | Alan (x, z) | En derin nokta | **Yaklaşma noktası** (kenar) | Not |
|---|---|---|---|---|---|
| 1 | 1820 | x 1268–1636, z 1164–1376 | (1300, 1296) y=−7,4 | (1280, 1280) y=−0,1 | Karus kapısının kuzeyi |
| 2 | 1225 | x 404–720, z 692–820 | (424, 820) y=−7,1 | (440, 820) y=0,5 | |
| 3 | 247 | x 812–856, z 884–1064 | (824, 952) y=−5,9 | (812, 952) y=−0,1 | **bowl içinde (batı)** |
| 4 | 228 | x 1164–1236, z 940–1112 | (1168, 1108) y=−2,4 | (1172, 1112) y=−0,3 | **bowl içinde (doğu)** |
| 5 | 219 | x 424–512, z 1096–1220 | (472, 1204) y=−3,4 | (472, 1228) y=0,2 | |

Kayıttaki `y` (yükseklik) değeri, içine girdiysen ne kadar indiğini gösterir: y −1 m'nin altına inmediysen çukura giremedin demektir.

## 6. Eğim yerleri (hepsi bowl'a yakın, 12 m'lik düz çizgi; eğim = yükseklik farkı / 4 m)

Sınır değerleri: **0,625** (2,5 m/4 m: botun şu anki tahmini sınırı) ve **1,0** (4 m/4 m). Bantlar bu iki değeri sarar.

| # | Bant | Alt nokta | Üst nokta | Eğim | Sonuç (çıktı / yavaşladı / çıkamadı) |
|---|---|---|---|---|---|
| E1 | 0.30-0.45 | (1060, 1104) y=13.0 | (1072, 1104) y=16.9 | 0.33 | |
| E2 | 0.30-0.45 | (932, 1052) y=11.0 | (932, 1040) y=14.8 | 0.31 | |
| E3 | 0.30-0.45 | (912, 1016) y=20.5 | (900, 1016) y=24.4 | 0.32 | |
| E4 | 0.45-0.60 | (1048, 1160) y=9.5 | (1060, 1160) y=15.3 | 0.48 | |
| E5 | 0.45-0.60 | (1124, 924) y=11.0 | (1124, 912) y=17.0 | 0.50 | |
| E6 | 0.45-0.60 | (924, 1148) y=9.4 | (936, 1148) y=15.3 | 0.50 | |
| E7 | 0.60-0.70 | (1048, 1168) y=7.2 | (1060, 1168) y=15.0 | 0.65 | |
| E8 | 0.60-0.70 | (928, 1152) y=9.0 | (928, 1140) y=16.3 | 0.61 | |
| E9 | 0.60-0.70 | (1052, 816) y=0.7 | (1052, 828) y=8.5 | 0.65 | |
| E10 | 0.70-0.85 | (960, 1168) y=8.3 | (960, 1156) y=18.2 | 0.82 | |
| E11 | 0.70-0.85 | (1176, 1084) y=-1.2 | (1176, 1072) y=8.3 | 0.80 | |
| E12 | 0.70-0.85 | (976, 808) y=3.5 | (976, 820) y=13.0 | 0.80 | |
| E13 | 0.85-1.10 | (1100, 1076) y=12.8 | (1100, 1088) y=24.0 | 0.93 | |
| E14 | 0.85-1.10 | (916, 1092) y=9.5 | (904, 1092) y=20.3 | 0.90 | |
| E15 | 0.85-1.10 | (1104, 1156) y=7.5 | (1092, 1156) y=18.8 | 0.94 | |
| E16 | >=1.10 | (1152, 1104) y=7.7 | (1140, 1104) y=26.4 | 1.56 | |
| E17 | >=1.10 | (1116, 1148) y=8.2 | (1116, 1136) y=23.2 | 1.25 | |
| E18 | >=1.10 | (872, 876) y=13.7 | (884, 876) y=30.4 | 1.40 | |

(Liste `python3 tools/slope-candidates.py --per-band 3` çıktısıdır; yer değiştirmek istersen `--near X,Z` ile başka bir noktaya yakın üretirim.)

## 7. Not tablosu (doldur)

| Saat (başlangıç–bitiş) | Irk | Bölüm | Etiket | Notlar (ne gördün, nerede durdun, neyi denedin) |
|---|---|---|---|---|
| | | | | |

## 8. Bitirince

```bash
tools/trace-session.sh finish                 # izleyicisiz normal derlemeye döner, sunucuları kapatır
git switch gece/2026-10-02                    # döngünün dalı
rm -f plans/.supervisor-stop plans/.auto-loop-stop
```
Sonra bana **not tablosunu** ver (ya da yaz), `plans/_logs/trace/` içindeki etiketlerin hazır olduğunu söyle. Ben şunları yaparım: `python3 tools/route-extract.py <kayıt> --name <karakter> --default-zones` (rota, durma, hız, bölge ziyaretleri), eğim yerlerinde konum ilerlemesi ve yükseklikten çıktı/çıkamadı kararı, çukurlarda ulaşılan en düşük `y`, sonucu `docs/12` §13.1'e ve T-NAV-02/T-NAV-09 satırlarına işlerim, F5-67 için karar öneririm. Döngüyü yeniden başlatmak için `/tmp/claude-1000/supervisor/auto-supervisor.sh` çalıştırılır (izin sende).
