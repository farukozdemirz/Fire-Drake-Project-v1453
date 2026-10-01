---
name: plan-dogrula
description: DeepSeek'in (opencode) uyguladığı bir plans/ planını denetler. Plan kapsamını, kabul kriterlerini, derlemeyi, proje kurallarını ve kod kalitesini kontrol eder; plan dosyasına Doğrulama Raporu ve gerekirse DeepSeek'e verilecek düzeltme talimatı yazar.
argument-hint: "<plans/FAZ-NN-ad.md>"
disable-model-invocation: true
---

# Plan doğrula

Doğrulanacak plan: `$1`. Boşsa `plans/README.md`'de durumu `UYGULANDI` olan en eski planı seç ve kullanıcıya hangisini seçtiğini söyle.

Sen denetçisin. Amaç kodu "onaylamak" değil, planın gerçekten ve kurallara uygun biçimde yapıldığını **kanıtla** göstermek ya da eksikleri bulmaktır. Uygulayıcının raporundaki iddiaları doğrulamadan kabul etme.

## 1. Hazırlık

1. Plan dosyasını tamamen oku: kabul kriterleri, dokunulabilecek dosyalar, kapsam dışı, Uygulayıcı Raporu'nun son turu.
2. Planın durumu `UYGULANDI` değilse dur ve kullanıcıya bildir.
3. Branch ve farkları çıkar:
   - `git log --oneline main..bot/<FAZ>-<NN>`
   - `git diff --stat main...bot/<FAZ>-<NN>`
   - `git diff main...bot/<FAZ>-<NN>`
   - Branch yoksa veya commit yoksa bu bir bulgudur.
4. Çalışma ağacında commit edilmemiş değişiklik var mı bak (`git status`). Commit edilmemiş iş doğrulanmaz.

## 2. Denetim

Ayrıntılı liste: [kontrol-listesi.md](kontrol-listesi.md). Her maddeyi uygula. Özetle:

1. **Kapsam:** Değişen her dosya izin listesinde mi? Kapsam dışı iş, gereksiz yeniden düzenleme, silinen kod var mı?
2. **Kabul kriterleri:** Her kriteri **kendin** doğrula; uygulayıcının ✔ işaretine güvenme. Doğrulama komutla, kodun ilgili satırını okuyarak (dosya:satır) veya ölçümle yapılır.
3. **Derleme:** Yapı oturumu geçici branch'e geçirmeyi gerektiriyorsa önce kullanıcıya söyle.
   - `git switch bot/<FAZ>-<NN>`
   - `./tools/build.sh Release`, plan isterse `Debug` da
   - Sonra eski branch'e dön.
   - Hatalar ve **yeni** uyarılar not edilir.
4. **Proje kuralları** (`AGENTS.md` §2–3, `docs/03` §13, `docs/13` §3):
   - mekanik değişikliği
   - bot avantajı
   - thread kuralı
   - DB kuralı
   - dosya kodlaması / satır sonu / BOM
   - vcxproj kaydı
   - bot sistemi varsayılan kapalı mı
5. **Kod kalitesi:**
   - doğruluk hataları (sınır koşulları, null, kilit, sızıntı)
   - çevredeki koda uyum
   - gereksiz karmaşıklık
6. **Dürüstlük:** Rapordaki iddialar (derleme çıktısı, "test geçti") gerçekle uyuşuyor mu?

## 3. Karar

| Karar | Ne zaman |
|---|---|
| `DOĞRULANDI` | Tüm kabul kriterleri kanıtla karşılandı, derleme temiz, kural ihlali yok. Küçük üslup notları engel değildir; rapora "not" olarak yazılır. |
| `DÜZELTME GEREKLİ` | En az bir kriter karşılanmadı, hata veya kural ihlali var, ya da kanıt eksik. |
| `REDDEDİLDİ` | İş planla ilgisiz veya baştan yapılması gerekiyor. Gerekçeyle kullanıcıya danış. |

## 4. Raporu yaz

Plan dosyasının "Doğrulama Raporu" bölümüne yeni `### Tur N — <tarih>` ekle; önceki turları silme. İçerik:

- karar
- incelenen commit (`git rev-parse --short bot/<FAZ>-<NN>`)
- kriter tablosu (sonuç + kanıt)
- önem sırasına göre bulgular (her biri `dosya:satır` ile)
- `DÜZELTME GEREKLİ` ise **düzeltme talimatı:** DeepSeek'e aynen verilecek, numaralı, her madde tek ve somut iş

Düzeltme talimatı şu satırla başlar:

```
plans/<FAZ>-<NN>-<ad>.md — Doğrulama Turu N düzeltmeleri. Aynı branch'te yalnızca şunları yap, sonra raporuna "Tur N+1" ekle:
```

Planın `Durum` satırını kararla güncelle. **Kodu kendin düzeltme.** Uygulama DeepSeek'in işidir. Kullanıcı açıkça isterse istisna olur.

## 5. Kayıtları güncelle

1. `plans/README.md` listesinde durumu güncelle.
2. `docs/STATUS.md`:
   - "Son doğrulamalar" tablosu
   - blokajlar
   - sıradaki adımlar
3. Yeni bilinen sorun bulduysan `docs/KNOWN_ISSUES.md`'ye ekle (`docs/21` §4.4).
4. Doğrulama bir mekanik bilgiyi kesinleştirdiyse ilgili dokümanı güncelle. Etiket yükseltme kuralı `docs/21` §5'tedir.
5. Faz kapısı: fazın tüm planları `DOĞRULANDI` ise kullanıcıya faz sonuç raporu (`docs/templates/PHASE_REPORT.md`) hazırlamayı öner. Faz `KABUL_EDILDI` ancak kullanıcı onayıyla yazılır.

6. **Commit:** Plan dosyası, `plans/README.md`, `docs/STATUS.md` (ve değiştirdiysen `docs/KNOWN_ISSUES.md`, ilgili doküman) değişikliklerini **plan branch'ine** commit et: mesaj `[<FAZ>-<NN>] Doğrulama raporu: <karar>`. Yalnızca bu dosyaları `git add` ile ekle (`git add -A`/`.` yok). Commit edilmemiş rapor, sıradaki planın dalı açılırken kaybolur/çakışır.
7. **Otonom mod:** `echo "${AUTO_LOOP:-0}"` `1` ise kullanıcıya soru sorulamaz. Karar `DÜZELTME GEREKLİ` ise düzeltme talimatını mutlaka plan dosyasındaki "Düzeltme talimatı" bloğuna **kod çitiyle** yaz (döngü oradan okur). `REDDEDİLDİ` kararını yalnızca gerçekten baştan yapılması gerekiyorsa ver; belirsizlikte `DÜZELTME GEREKLİ` tercih et. `plans/.aktif-plan`'a dokunma.

## 6. Kullanıcıya özet

Kararı, kriter sonuçlarını (kaç ✔ / ✘) ve en önemli 3 bulguyu yaz.

- `DÜZELTME GEREKLİ` ise düzeltme talimatını kopyalanabilir blok olarak tekrar ver.
- `DOĞRULANDI` ise birleştirme komutunu öner ama birleştirmeyi kullanıcı onaylamadan yapma:

```bash
git switch main && git merge --no-ff bot/<FAZ>-<NN>
```
