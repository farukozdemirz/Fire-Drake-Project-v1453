# ADR-0019: Geri çekilme eşiği ve yeniden savaşa dönüş eşiği (Q-29)

Durum: KABUL
Tarih: 2026-10-03 · Karar verenler: proje sahibi (soru-cevap oturumu)
İlgili: F6-05 (geri çekilme, iyileşme, yeniden giriş), F6-06 (warrior solo uçtan uca), `docs/11` §4, `docs/10`, açık soru Q-29

## Bağlam

Proje sahibinin ilk uçtan uca hedefi: warrior bot hedef seçer, navigasyonla yaklaşır, skill + R ve pot kullanır, **HP %30'un altına düşünce geri çekilir**, iyileşince yeniden savaşa katılır. `docs/11` §4.1 bunu "HP %30" olarak yazar `[D]`; §4.2 ise eşiği tehdit, destek ve kök etkisine göre değiştiren bir formül verir (`0,30 + role_adj + 0,05·threat − 0,08·support + 0,1·[kök]`; 1v1 melee yakınken 0,35) `[D]`. Yeniden giriş eşiği iki belgede farklıdır: `docs/11` 0,65, `docs/10` 0,85 `[D]` (`docs/reports/plan-bagimlilik-F6-2026-10-03.md` §4 madde 2).

## Karar

1. **Geri çekilme eşiği sabit %30'dur** (formülde `threatWeight = 0`, `role_adj = 0`, `support` ve kök terimleri devre dışı): ilk uçtan uca koşuda ve F6'nın ilk kabulünde sabit taban kullanılır. `docs/11` §4.2 formülü ölçümlerden sonra, ayrı bir kararla devreye alınabilir.
2. **Yeniden savaşa dönüş eşiği %85'tir** (`REENTER = 0,85`; `docs/10` değeri esas). Geri çekilme (%30) ile yeniden giriş (%85) arasındaki geniş aralık, kaç-dön salınımını önler.

## Değerlendirilen alternatifler

| Alternatif | Artılar | Eksiler | Neden seçilmedi |
|---|---|---|---|
| `docs/11` §4.2 formülü | Tehdide ve desteğe duyarlı, 1v1 melee'de %35 | İlk koşuda ayarlanacak parametre ve hata kaynağı artar; ölçüm olmadan değerleri `[A]` | İlk koşu basit ve ölçülebilir kalsın |
| Yeniden giriş %65 | Bot daha çabuk döner | %30 eşiğiyle aralık dar, salınım riski | Güvenlik ve salınımsızlık tercih edildi |

## Sonuçlar

Olumlu: ilk uçtan uca koşu anlaşılır ve tekrarlanabilir (`T-IGT-WAR-01`, `T-SUR-*`). Olumsuz: sabit eşik tehdide göre ayarlanmaz; güçlü düşmanlara karşı geç, zayıf düşmanlara karşı erken kaçabilir. Risk: `docs/11` §4.2 ile F6-05 arasında geçici fark (planda `[A]` olarak yazılır). Geri alma: eşik `BrainParams` parametreleridir (kodda değer değişimi, davranış bayrağı).

## Doğrulama

F6-05 birim testleri (eşik sınırları, histerezis: 0,30 altında RETREAT, 0,85 üstünde yeniden giriş, aradaki bölgede salınım yok) ve F6-06 `S4` oyun içi koşusu (Claude): HP %30'un altına düşen bot geri çekilir, %85'e çıkınca döner; en az 10 tekrarda gidip gelme sayısı 0.
