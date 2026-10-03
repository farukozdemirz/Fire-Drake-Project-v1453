# ADR-0022: Kapsam ve quest kararları: rogue/okçu kapsam dışı, bot quest kurulumu onayı, Q-28 mevcut hâl

Durum: KABUL
Tarih: 2026-10-03 · Karar verenler: proje sahibi (soru-cevap oturumu)
İlgili: ADR-0010 (K-7), ADR-0018 Ek 3 ve Ek 7, F4-27, `db/003_bot_quests.sql`, KI-017, KI-018, açık soru Q-28

## Bağlam

(1) ADR-0018 Ek 7 rogue/okçu botlarını (Type2, Type9) ertelemişti, bu hiç onaylanmamıştı. (2) `db/003` botların quest listesine sınıflarının quest'lerini "tamamlandı" yazar; ADR-0010 (K-7) görev kapılı master skill'lerin ilk sürümde kullanılmamasını ve görevlerin betikle tamamlanmış sayılmamasını kararlaştırmıştı. (3) 51–54 quest kimlikli 32 skill'in şartı sunucuda `UseStanding` sütununda duruyor; sunucu bunları kilitlemiyor, yalnızca istemci kilitliyor (Q-28).

## Karar

1. **Rogue ve okçu botları kapsam dışıdır.** Warrior, priest ve mage ile devam edilir; Type2 (okçu), Type9 (gizlilik) ve ilgili skill türleri yapılmaz, F4 kapsamı bu yönüyle kapanır. İstenirse F6 sonrası ayrı bir kararla yeniden açılır.
2. **Bot quest kurulumu (`db/003`) onaylandı.** ADR-0010 (K-7) şu şekilde güncellenir: görev durumu **yalnızca bot karakter satırlarına, açık ad listesiyle ve geri alınabilir** (`db/003` + rollback) yazılabilir; gerçek oyuncu kayıtlarına ve oyun mekaniğine dokunulmaz. Betik şu an veritabanına uygulanmamıştır; ölçüm için geçici uygulanıp geri alınabilir, kalıcı uygulama ayrı bir adımdır.
3. **Q-28 (51–54 quest'lerini sunucuda zorunlu kılma): şimdilik mevcut hâl.** Hiçbir veri değişmez. Önce salt-okunur bir ölçümle gerçek oyunda quest durumunun neye ulaştığı görülür; karar sonra verilir. Bu karar bot ilerlemesini engellemez.

## Değerlendirilen alternatifler

| Konu | Reddedilen | Neden |
|---|---|---|
| 1 | Rogue/okçu planla | Belirgin ek iş (yeni skill türleri, profiller); F6/F7'yi geciktirir |
| 2 | `db/003` reddedilsin, K-7 aynen kalsın | Botların sınıf skill'leri açılması istendi; bugün pratik etkisi yok ama ileride gerekir |
| 3 | `Etc`'e taşı | Gerçek oyuncu quest durumu doğrulanmadı (Q-04); meşru oyuncular skill atamaz hâle gelebilir |

## Sonuçlar

Olumlu: kapsam netleşir; bot skill erişimi geri alınabilir şekilde açık; gerçek oyuncu mekaniği korunur. Olumsuz: Q-28'deki sunucu tarafı hile açığı (KI-017) şimdilik kalır. Geri alma: `db/003_bot_quests_rollback.sql`.

## Doğrulama

`db/003` uygulandığında `BOTQUEST: rows=12 ok=12 fail=0`; Q-28 için salt-okunur ölçüm sonucu ayrı kayıtla gelir.
