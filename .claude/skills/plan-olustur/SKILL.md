---
name: plan-olustur
description: Fire Drake PK bot projesi için DeepSeek'in (opencode) uygulayacağı bir sonraki iş planını plans/ altına yazar. Kullanıcı yeni plan, sıradaki iş veya bir fazın planlanmasını istediğinde kullanılır.
argument-hint: "[faz veya konu, ör. F0 | F2-slot-havuzu]"
disable-model-invocation: true
---

# Plan oluştur

Görev: `$1` için (boşsa `docs/STATUS.md`'deki "Sıradaki adımlar"dan ilk uygun iş) **tek bir uygulanabilir plan** yazmak. Planı okuyacak olan DeepSeek'tir. DeepSeek projeyi bilmez, yalnızca `AGENTS.md`'yi ve bu planı okur. Plan ondan tahmin istememeli.

## 0. Otonom mod

`echo "${AUTO_LOOP:-0}"` çalıştır. `1` ise bu çağrı **insansız** döngüden (`tools/auto-loop.sh`) geliyor: kullanıcıya soru **sorulamaz** (soru sormaya çalışırsan süresiz asılı kalırsın) ve çıktını kimse anında okumaz. O zaman:

- **Tasarım/içerik kararı gerekirse** (`docs/adr/` altında yeni ADR gerektiren türden) kullanıcıya sorma: önerilen seçeneği sen seç, ADR'yi yaz ve başlığına `(otonom döngüde Claude kararı — gözden geçirilmeli)` ekle, `docs/STATUS.md` "Verilen kararlar"a tek satır ekle. Dayanak sırası: `docs/18` §1'deki mevcut kararlar → `docs/17` → önerilen seçenek.
- **Geri alınamaz/dışa dönük eylemlere** (push, merge, faz `KABUL_EDILDI`) hiçbir zaman gitme; bunlar zaten yasak.
- **Faz sınırı:** Önceki planlar bu fazın **tüm** işlerini bitirdiyse (`docs/17` §2'deki faz kapsamı ve çıkış koşulları karşılandıysa) veya sıradaki iş **başka bir fazın** işiyse: yeni plan **yazma**. `docs/templates/PHASE_REPORT.md`'den faz sonuç raporu taslağını `docs/phase-reports/<FAZ>-taslak.md` olarak yaz, `docs/STATUS.md` "Blokajlar"a `faz onayı bekliyor: <FAZ>` satırı ekle, `plans/.aktif-plan`'a **dokunma** ve dur. (Döngü bunu "plan yazılmadı" olarak görüp durur.)
- **Elle yapılması gereken bir iş kaldıysa** (ör. insan istemcisiyle oyuna giriş) bunu planın "Kapsam dışı" ve `docs/STATUS.md` "Blokajlar" bölümüne yaz; planı yalnızca DeepSeek'in yapabileceği kısımla sınırla.

`AUTO_LOOP` 1 değilse kullanıcı karşındadır: kararları ona **tek tek ve sade dille** sor (bölüm 5.3).

## 1. Durumu öğren

1. `docs/STATUS.md`, `plans/README.md` (plan listesi) ve aktif fazın `docs/17_IMPLEMENTATION_ROADMAP_AND_PHASE_GATES.md` tanımını oku.
2. Plan konusunun ilgili dokümanlarını oku (`docs/00` §1 sahiplik tablosundan bul). Mekanik için `docs/03`, mimari için `docs/13`, entegrasyon için `docs/02` §11.
3. Bağımlı planlar `DOĞRULANDI`/`KAPANDI` değilse planı `TASLAK` bırak ve kullanıcıya söyle (otonom modda: yazma, `Blokajlar`a ekle, dur).
4. `git branch --show-current` ve `git status --short` bak. Çalışma ağacı kirliyse (commit edilmemiş iş) yeni plan yazma; durumu raporla.

## 2. Kodu kendin doğrula

Planda adı geçecek her fonksiyonu, satırı ve sabiti **depoda aç ve doğrula**. Bunu dokümana güvenmeden yap; satırlar kaymış olabilir.

- ISO-8859 dosyalarda `grep -a` kullan.
- Bulduğun gerçek `dosya:satır` değerlerini plana yaz.
- Doğrulayamadığın bir şeyi planda kesin bilgi gibi yazma; "uygulayıcı önce şunu kontrol etsin" adımı ekle.

## 3. Planı boyutlandır

- Bir plan ≤ ~1 günlük iş ve ≤ ~10 dosya olmalı. Daha büyükse birden çok plana böl (her biri ayrı dosya, bağımlılıkları belirtilmiş).
- Her planın sonunda proje **derlenebilir** ve bot sistemi kapalıyken davranış **değişmemiş** olmalı.

## 4. Dosyayı yaz

- `plans/_SABLON.md`'yi kopyalayarak `plans/<FAZ>-<NN>-<kisa-ad>.md` oluştur. NN, o fazdaki sıradaki numaradır.
- **Taban branch (zincirleme dallar):** Birleştirmeyi yalnızca proje sahibi yapar. Bu yüzden önceki plan `DOĞRULANDI` ama `main`'e **birleşmemişse** (`git branch --merged main` içinde değilse), yeni planın "Branch" alanına `bot/<FAZ>-<NN> (taban: bot/<önceki-plan>)` yaz. Önceki plan birleşmişse taban `main`. Böylece DeepSeek önceki işin kodunu görür. Bağımlı değilse ama yine birleşmemiş dallar varsa yine zincirle (sıra bozulmasın).
- Şablondaki tüm bölümleri doldur. Özellikle:
  - **Dokunulabilecek dosyalar:** tam liste. Yeni `.cpp`/`.h` dosyaları için `GameServer/proj-GameServer.vcxproj` ve `.filters` satırlarını da ekle.
  - **Uygulama adımları:** numaralı ve somut. Fonksiyon imzalarını, veri yapılarını ve paket alanlarını ver. Gerekirse kısa kod iskeleti ekle ama uygulamayı DeepSeek'e bırak.
  - **Kabul kriterleri:** her biri komutla, dosya:satırla veya ölçümle doğrulanabilir olmalı. `tools/build.sh Release` hatasız kriteri her planda bulunur.
  - **Kapsam dışı:** DeepSeek'in "iyileştirme" yapıp kapsamı büyütmesini önleyecek açıklıkta yaz.
  - **Kısıtlar:** ilgili `MEC-*`, `CLI-*` kuralları, thread kuralı, kodlama/satır sonu uyarıları.
- Durum: tüm bölümler tamamsa `HAZIR`, değilse `TASLAK`.

## 5. Kayıtları güncelle

1. `plans/README.md` plan listesine satır ekle.
2. `docs/STATUS.md`'de aktif faz, plan ve sıradaki adımları güncelle.
3. Plan yeni bir karar gerektiriyorsa: kullanıcı karşındaysa kendin karar verme, **tek tek ve sade dille** sor, cevabı `docs/adr/` altına yaz. Otonom moddaysan bölüm 0'a göre karar ver.
4. Plan `HAZIR` ise `plans/.aktif-plan` dosyasına planın yolunu yaz (tek satır, örn. `plans/F0-02-sunucu-baslatma.md`, sonda boş satır olmadan: `printf '%s' "plans/..." > plans/.aktif-plan`). `TASLAK` ise yazma.
5. Yaptığın değişiklikleri **mevcut branch'e commit et** (yeni plan dosyası, README, STATUS, varsa ADR): mesaj `[<FAZ>-<NN>] Plan yazıldı: <kısa ad>`. `git add` ile yalnızca bu dosyaları ekle; `git add -A`/`.` kullanma. `plans/.aktif-plan` ve `plans/_logs/` git'e girmez (`plans/.gitignore`).

## 6. Kullanıcıya ver

Kısa özet ver: plan dosyası yolu, ne yapacağı, kaç adım ve hangi kriterler. Ardından opencode'a verilecek tek satırlık komutu yaz:

```
plans/<FAZ>-<NN>-<kisa-ad>.md planını AGENTS.md kurallarına göre uygula.
```
