# ADR-0024: Botun yürünebilir eğim sınırı 0,45 (T-NAV-02)

Durum: KABUL
Tarih: 2026-10-03 · Karar verenler: proje sahibi (harita yürüyüş oturumu ve sonrası)
İlgili: F5-69, `docs/12` §2/§3, T-NAV-02, ADR-0006, `BotCore/NavGrid.h` `P-NAV-MAX-SLOPE`

## Bağlam

Botun yol planlayıcısı bir kenarı yalnızca `|Δh| ≤ maxSlope · mesafe` ise yürünebilir sayar; `maxSlope` 0,625 idi `[A]` (tahmin). T-NAV-02 (insan istemcisi, paket izleyici kaydı, karakterin kendi yüksekliği) şunu gösterdi `[V]`: 84 m'lik tasarlanmış rampa (ortalama 0,24, en dik yerel 0,47) çıkıldı; 0,78 yamaçta +4 m çıkılıp takılındı; 0,94 ve 1,26 çıkılamadı (iniş mümkün: en dik iniş 1,07). 0,54 ve 0,65 test edilmedi. Bot için 0,625 bu yüzden çıkılabildiğimizin üstünde kanıtsız bir değerdir.

## Karar

1. Botun yürünebilir saydığı en büyük kenar eğimi **0,45'tir** (1,8 m / 4 m hücre): sürekli çıkılan en dik yerel eğimin (0,47) hemen altı. Proje sahibi: "botlar herhangi bir nesneye takılmadan çıkabilsin, takılma problemiyle uğraşmaktansa rahatça çıkabilsin."
2. Kesin sınır (0,54–0,78 arası) aranmaz; sınır ileride ölçüm gerekirse ayrı bir kararla gevşetilebilir.
3. Tırmanış ve iniş şimdilik aynı kuralla (simetrik) işlenir; inişin daha dik olabilmesi bilinen bir fırsattır, yapılmaz.

## Değerlendirilen alternatifler

| Alternatif | Artılar | Eksiler | Neden seçilmedi |
|---|---|---|---|
| 0,625 aynen | değişiklik yok | çıkılamayan yamaçlara rota çizer, bot takılır | kanıtsız ve ölçümle çelişiyor |
| 0,25 | çok güvenli | haritanın %88'ine iner, rota +%97'ye kadar uzar | gereksiz kayıp |
| Kesin sınırı bul (0,54, 0,65 testleri) | daha az rota kaybı | ek insan testi, kazanç küçük | proje sahibi rahat/güvenli tercihi seçti |

## Sonuçlar

Olumlu: bot çıkılamayan yamaçlara girmez; takılma riski düşer. Olumsuz: Walk hücrelerinin %98,6'sı yerine %96,6'sı ulaşılabilir; rota uzaması Karus doğuş → arena +%11, El Morad doğuş → bowl +%25. Doğuş noktaları, arena, bowl ve kapılar bağlı kalır. Geri alma: `NavParams::maxSlope` varsayılanı.

## Doğrulama

F5-69 birim testi `NavGrid_DefaultSlope_045` ve gerçek harita bağlantı testi (`landmarks=5/5`); F5-66 çalışma zamanı koşusunda (doğuş → arena, takılma oranı).
