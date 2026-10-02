# F4-53: `Perception` dilim 11 — gözlenen buff/debuff/heal olayları tablosu (skill olaylarından)

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F4 — Aksiyon yürütme ve adalet koruması (`docs/17` §2; F7 priest/stall için ön koşul) |
| Branch | `bot/F4-53 (taban: gece/2026-10-02)` |
| Bağımlı olduğu planlar | **F4-52** (olay halkası) `KAPANDI` olmalı; F4-50/F4-51 önerilir; Type4 buff/debuff'ı **atabilmek** için ADR-0018 madde 4 dilimi (ana hat, F4-24 ve sonrası) ile doğrulama koşulur; `tools/check-perception-contract.py` R5 sözcük listesi (`buff`, `skill`) bu plan için ayrıca güncellenir |
| İlgili gereksinim / kabul | `docs/03` §16 (düşman üzerindeki buff/debuff görülen olaylardan takip edilir), `docs/07` §7.2/§9 (bitiş tahmini, debuff başarısı), `docs/09` §6.1 (`heal_rate`), `docs/13` §5.2a (`E` sınıfı: tahmin); DEG-08 |
| Tahmini büyüklük | M (6 kod dosyası; plan HAZIR yapılmadan önce yazım turu gerekir) |
| Hazırlayan / tarih | Claude / 2026-10-02 (değerlendirme) |

---

## 1. Amaç ve neden TASLAK

F4-52 olayları ham saklar. (`UnitView.statusCount` gibi `buff`/`skill` sözcüklü alanlar R5'e takılır: bu plan R5 politikasını da günceller.) Karar katmanı ise "hedefte Malice var mı, ne zaman biter", "hedefe son 5 sn'de kaç heal geldi", "dost warrior kök altında mı" bilmelidir. Bu bilgi **tahmindir** (`E` sınıfı): sunucu başkalarının buff listesini göndermez; yalnızca `EFFECTING` olayı (kim, kime, hangi skill) ve skill verisi (süre, tip) vardır. Sınıflandırma botun alabileceği **skill verisine** (MAGIC/MAGIC_TYPE3/4 tabloları: istemci de bunu bilir, `docs/05`) dayanır; bu, sunucu nesnesinden gerçek durum okumak değildir (`G` sınıfı yasak) ama tablo erişimi bir sözleşme denetimi gerektirir. Plan HAZIR yapılmadan önce yazılacak ayrıntılar:

- `GameServer/Bot/` tarafında salt-okunur bir **skill meta görünümü** (`SkillMeta { id, bType[2], moral, buffType (Type4), durationSec, isHeal(Type3 +HP), isHot, isDebuff }`) kaynağı: `m_MagictableArray` + `m_Magictype3Array/4Array` okuması dosya-statik tek yerde, `BotCore`'a **değer kopyası** olarak verilir (`BotCore` sunucu başlığı include etmez). Hangi alanların oyuncu istemcisinde de bulunduğu `docs/05` ve `docs/03` §16 ile doğrulanır.
- Tahmin kuralları: `EFFECTING` + Type4 buff/debuff → hedefte `(buffType, skillId, startMs, endMs = start + durationSec)`; aynı `buffType` yeni **debuff** eskisini (buff dahil) siler (`docs/05` §4, MEC-BUF-03); aynı `buffType` yeni **buff** reddedilir (olay gelmeyebilir); ölüm (`WIZ_DEAD`/OUT) tüm kayıtları siler (MEC-DTH-01); `Cure` (Type5) olayı hedefin debuff'larını siler; kaybolan olay (menzil dışı) bilinmeyen = "belirsiz" işareti.
- Heal olayları: Type3 HP+ olayları `heal_rate` penceresi için `HealObs {targetId, tMs, nominalAmount}` (nominal skill değeri; etkili miktar bilinmez).
- Tablo kapasitesi, bayatlık (`endMs` geçmiş), `/bot snap <bot> status` dökümü, `UnitView.statusCount` alanı.
- Doğrulama: bir bot diğerine Malice/Parasite/heal atarken (**önce ADR-0018 madde 4 Type4 cast desteği gerekir** veya mevcut destekli Type3 heal ile yalnız heal olayları) izleyici botun tablosu `list`/skill verisiyle çapraz doğrulanır.

## 2. Kapsam (taslak)

Yapılacak: skill meta görünümü, `ObservedStatusTable`, `HealObsRing`, `/bot snap <bot> status`, birim testleri. Kapsam dışı: karar/telemetri, takım paylaşımı, skill yürütme desteği (ADR-0018 dilimleri: ana hat F4-24 ve sonrası).

## 3. Kabul kriterleri (taslak)

- [ ] Birim: Type4 debuff sonrası bitiş = olay + süre; aynı tip debuff buff'ı siler; ölüm temizler; cure debuff'ları siler; heal penceresi toplamı
- [ ] Statik: yeni satırlarda sunucu nesnesi **durumu** (`m_buffMap`, `m_sHp`, ...) okunmaz; yalnızca skill **tablosu** (veri) ve alınan paketler
- [ ] Çalışma zamanı (Claude): izleyici botun gözlenen durumu, olayı üretenin gerçek buff listesiyle (teşhis `G`) tutarlı; bitiş tahmini ± 2 sn

## 4. Açık sorular

- Skill tablosu erişimi sözleşmeye uygun mu (istemci `.tbl` şifreli; sunucu tablosu istemci verisiyle aynı varsayımı `[A]`, Q-01 ile ilişkili).
- Alan skill'inde (`EFFECTING` hedefi `-1`) etkilenen hedeflerin tespiti: konum + yarıçap tahmini (ADR-0018 m.5 ile birlikte).
