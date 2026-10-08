# ADR-0069: Sürüm yükseltme tabanı AlphaGame 1534 (ADR-0068 madde 2, 4, 5 ve 6'nın yerine)

Durum: KABUL
Tarih: 2026-10-08 · Karar veren: proje sahibi ("biz alphagame'in base'ini ve skillerini vs kullanacağız. Bu yüzden onun betiklerini alalım ronark land haritasını vs. ben tekrardan kaydedeceğim."); Claude (uygulama sırası ve koruma kuralları)
İlgili: ADR-0068, `docs/reports/u0-1534/` A–G, KI-049, faz UA (`docs/17` §2 UA)

## Bağlam

- ADR-0068 seçenek A'yı (taban bizim kaynak, AlphaGame yalnız referans) seçmişti. Proje sahibi seçenek B'yi seçti: taban AlphaGame.
- Yeni kanıt (KI-049): AlphaGame `MAGIC` değerleri (`Range`, `ReCastTime`, `CastTime`) hem 1534 hem 1453 istemcisinin `Skill_Magic_Main_us.tbl` tablosuyla aynı; farklı olan bizim `MAGIC`'imiz `[V]`. AlphaGame skill verisi istemciyle tutarlı.
- Bilinen bedeller (A §0, §4–§7; C; D §0):
  - AlphaGame doğrulanmış mekaniklerimizden 11'ini değiştirir (A Ek A "BREAKS"); `docs/03` ve bot ayarlarının bir kısmı geçersizleşir.
  - Güvenlik: iki HIGH (isimle karakter ele geçirme, GM'siz ışınlanma) ve MEDIUM bulgular (A §6).
  - AIServer protokolü farklı; AIServer da AlphaGame'inki olur.
  - AlphaGame paket düzenleri bu istemciyle üç yerde uyuşmuyor `[C]`: MyInfo eşya listesi (74 gönderir, istemci 72 okur), ağırlık alanları (u16, istemci u32), görev 9/1 sayaçları (u8, istemci u16). AlphaGame'in 74 yuvalı/8 cospre envanter modeli bu istemcide yok (D §0 madde 3–4).
  - AlphaGame DB'si: `ITEM.ItemClass` her satırda NULL, `KNIGHTS_CAPE`'te 144 satır yanlış, kendi koduyla uyumsuz prosedürler, SQL 2019 (C §2.4, §3.5; E §2).
  - AlphaGame `moradon_0826.smd` bozuk (yükseklik transpoze, olay ızgarası sıfır; B §3).
  - AlphaGame `freezone_b.smd` (zone 71) istemcinin Ronark arazisiyle uyuşmuyor: yüksekliklerin %73'ü aynı, en çok 52 m fark, bowl'da 1.217 olay hücresi farklı (B §1.2). Bizim `freezone_a_20050718.smd` istemciyle %100 aynı.

## Karar

1. **Taban:** AlphaGame kaynak kodu (`GameServer`, `AIServer`, `LogInServer`, `shared`, `N3BASE`, `scripting`). Bot katmanı (`BotCore`, `GameServer/Bot`, `Tests`, `tools`, `bots`) bu tabana taşınır (A §5: 16 dosya, 42 hunk).
2. **Koruma kuralları (değişmez):**
   - Paketteki `exe/lib/pdb/idb` dosyaları kullanılmaz ve bağlanmaz; her şey kaynaktan derlenir.
   - A §6 #1, #2, #4, #5, #6 ve giriş günlüğünde parola (KI-043 eşdeğeri) kapatılmadan AlphaGame kodu çalıştırılmaz.
   - Sabit kodlu SQL kimlik bilgileri kaldırılır; paketle gelen günlükler kopyalanmaz ve dağıtılmaz.
3. **İstemci düzeltmeleri:** D raporunun `[C]` bulguları ve U1-02..U1-07'de istemciden doğrulanan düzenler AlphaGame koduna uygulanır (MyInfo 72 eşya, u32 ağırlıklar, görev sayaçları u16, 73 yuvalı envanter, klan fonu alanı, pelerin rengi kaynağı).
4. **Veri:** AlphaGame DB'si taban; `MAGIC`/`MAGIC_TYPE*` AlphaGame'inki (ADR-0068 madde 4 kalkar). Bilinen kusurlar betikle düzeltilir: `ItemClass` (E §3.4 kuralı), `KNIGHTS_CAPE` (istemci `Cloak.tbl`, E §2), prosedür uyumsuzlukları (C §2.4), bot satırları (`db/002`–`db/011` uyarlaması). Oyun sunucusu `.\SQL2019` örneğinde çalışır (ADR-0068 madde 6 kalkar).
5. **Görevler:** AlphaGame Lua betikleri ve `QUEST_HELPER` (istemciyle birebir, E §6.1).
6. **Haritalar:** AlphaGame `Map` seti, iki istisna ve bir kabul edilmiş risk:
   - Zone 21: AlphaGame dosyası bozuk; U3-01'de istemciden üretilen `moradon_1534.smd` kalır.
   - Zone 71: proje sahibi kararıyla AlphaGame `freezone_b.smd`. **Kabul edilen risk:** sunucunun zemini istemcinin çizdiği zeminden farklıdır (yükseklik, yürünebilirlik). Bot rotalarının ve gezinme verisinin yeniden üretilmesi botları düzeltir, bu farkı düzeltmez. İnsan testinde bowl ve kapılar ayrıca denenir. Geri dönüş tek dosya değişimidir (`freezone_a_20050718.smd`).
   - Bot gezinme verisi (`NavService` parmak izi, `RoamRouteData`) yeni haritaya göre yeniden üretilir; proje sahibi rota kaydını yeniden yapar.
7. **Mekanik:** önce `docs/03` AlphaGame kurallarına göre güncellenir, sonra bot ayarları; botlar yeniden doğrulanır (T-UPG-04).
8. **Hat:** yeni entegrasyon dalı `yukseltme/alpha` (taban `yukseltme/1534`); plan dalları `bot/UA-NN`. Canlı 1453 ortamı (`FDP_kn_online`) ve 1534 test ortamı (`FDP_kn1534`, `C:\dev\fdp1534`) bu faz bitene kadar olduğu gibi kalır.

## Uygulama sırası (faz UA)

| Plan | İş |
|---|---|
| UA-01 | AlphaGame kaynağını içe aktarma ve Release\|Win32 derleme (bot katmanı devre dışı, sunucu çalıştırılmaz) |
| UA-02 | Güvenlik düzeltmeleri (madde 2) |
| UA-03 | İstemci düzen düzeltmeleri (madde 3) |
| UA-04 | Bot katmanı kancaları ve BotCore ayrıştırıcıları |
| UA-05 | DB tabanı ve düzeltme betikleri (`.\SQL2019`) |
| UA-06 | Map, Lua ve `QUEST_HELPER` dağıtımı |
| UA-07 | `docs/03` güncellemesi, bot yeniden doğrulama, gezinme verisi |

## Değerlendirilen alternatifler

ADR-0068'deki tablo geçerlidir; seçilen artık B'dir. Ronark için alternatif (bizim `freezone_a_20050718.smd`'yi korumak) Claude tarafından önerildi; proje sahibi AlphaGame haritasını seçti.

## Sonuçlar

- Olumlu: istemciyle tutarlı skill, görev ve eşya verisi; AlphaGame ek sistemleri (Genie, BDW, Juraid, Chaos, VIP depo, mühür, balıkçılık/madencilik) gelir.
- Olumsuz: 11 mekanik kural değişir; bot ayarları ve `docs/03`'ün ilgili satırları yeniden doğrulanmalı; `docs/02`/`docs/03` satır referansları AlphaGame koduna göre yeniden yazılmalı; güvenlik düzeltmeleri zorunlu.
- Geri alma: `yukseltme/1534` hattı ve ADR-0068 düzeni (bizim taban + `FDP_kn1534`) bu faz boyunca çalışır durumda kalır.

## Doğrulama

- T-UPG-01/02/03 AlphaGame tabanıyla yeniden.
- T-UPG-04: bot birim testleri ve 6v6/8v8 senaryoları yeni tabanda.
- Güvenlik: A §6 bulgularının her biri için kod alıntılı kapanış kanıtı.

## Ek 1 (2026-10-08): AlphaGame dosyalarının bayt bayt saklanması

- UA-01 ile `.gitattributes`'a `GameServer/** AIServer/** LogInServer/** shared/** N3BASE/** scripting/** -text` eklendi (`GameServer/Bot/**` ve `shared/ProtocolProfile.h` depo varsayılanında). Sebep: AlphaGame dosyalarında LF, CRLF ve karışık satır sonu var; `core.autocrlf=true` bunları dönüştürüp `tools/ua-import-alpha.py --check` denetimini bozuyordu.
- Kural: bu dizinlerde dosya düzenleyen, dosyanın özgün satır sonunu ve kodlamasını korur.
- Tuzak: kural gelmeden önce açılmış bir çalışma ağacı, birleştirmeden sonra bu dizinlerde dönüştürülmüş dosyalar taşıyabilir (`git status` temiz görünür). Çözüm: ağaç temizken bu dizinlerin dosyalarını silip `git checkout -- <dizinler>`; sonra `--check` `PASS` vermeli.
- `shared/ProtocolProfile.h` tutulur (BotCore testleri kullanıyor).
