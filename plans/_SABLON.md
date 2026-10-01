# <FAZ>-<NN>: <Plan başlığı>

| Alan | Değer |
|---|---|
| Durum | TASLAK |
| Faz | F<n> — <faz adı> (`docs/17` §2) |
| Branch | `bot/<FAZ>-<NN>` (taban: `main`) |
| Bağımlı olduğu planlar | — |
| İlgili gereksinim / kabul | REQ-…, AC-…, T-… |
| Tahmini büyüklük | S / M (bir plan ≤ ~1 gün iş, ≤ ~10 dosya) |
| Hazırlayan / tarih | Claude / YYYY-MM-DD |

---

## 1. Amaç

Bu plan tamamlandığında ne değişmiş olacak (1–3 cümle).

## 2. Bağlam (okunması zorunlu)

- `docs/<dosya>.md` §… — neden önemli (tek satır)
- İlgili kod: `GameServer/…:satır` — ne yapıyor

## 3. Kapsam

**Yapılacaklar**

- …

**Kapsam dışı (yapılmayacak)**

- …

## 4. Dokunulabilecek dosyalar

| Dosya | İşlem | Not |
|---|---|---|
| `GameServer/Bot/…` | yeni | |
| `GameServer/User.h` | değiştir | yalnızca … |

Bu listede olmayan bir dosyaya dokunmak gerekirse **durup** Uygulayıcı Raporu'nda soru olarak yaz.

## 5. Uygulama adımları

1. …
2. …

## 6. Kabul kriterleri

Her madde doğrulanabilir olmalı (komut, dosya:satır, ölçüm).

- [ ] K1: …
- [ ] K2: `tools/build.sh Release` hatasız biter (yeni uyarı yok)
- [ ] K3: …

## 7. Doğrulama komutları

```bash
./tools/build.sh Release
git diff --stat main...bot/<FAZ>-<NN>
```

## 8. Kısıtlar ve uyarılar

- Kodlama/satır sonu: `AGENTS.md` §Kod kuralları.
- Bu plana özgü riskler: …

---

## Uygulayıcı Raporu (DeepSeek doldurur)

### Tur 1

- Durum: UYGULANDI
- Branch / commit'ler: `bot/<FAZ>-<NN>` — `<kısa-sha> [<FAZ>-<NN>] …`
- Değişen dosyalar ve neden:
  - `…`
- Derleme sonucu (`tools/build.sh Release` son 10 satır):
  ```
  …
  ```
- Kabul kriterleri öz-değerlendirme: K1 ✔/✘ …
- Plandan sapmalar ve gerekçeleri: …
- Açık sorular: …

---

## Doğrulama Raporu (Claude doldurur, `/plan-dogrula`)

### Tur 1 — YYYY-MM-DD

- Karar: DOĞRULANDI / DÜZELTME GEREKLİ / REDDEDİLDİ
- İncelenen: `main...bot/<FAZ>-<NN>` @ `<sha>`
- Kriter sonuçları:

| Kriter | Sonuç | Kanıt |
|---|---|---|
| K1 | ✔ / ✘ | dosya:satır / komut çıktısı |

- Bulgular (önem sırasıyla):
  1. …
- Düzeltme talimatı (DeepSeek'e aynen verilecek):

```
…
```
