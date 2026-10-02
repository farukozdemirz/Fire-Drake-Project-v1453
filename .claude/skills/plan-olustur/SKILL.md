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

### 0.1 Gece modu (`AUTO_INTEGRATION_BRANCH` dolu)

`echo "$AUTO_INTEGRATION_BRANCH $AUTO_TARGET_PHASE"` çalıştır. `AUTO_INTEGRATION_BRANCH` doluysa (ör. `gece/2026-10-02`) döngü **gece modunda**: doğrulanan plan dalları bu entegrasyon dalına **döngü betiği tarafından** birleştirilir; `main`'e dokunulmaz. Bu modda yukarıdaki kuralların şu farklarla uygulanır (bunlar önceliklidir):

- **Dal:** Plan bu dala commit edilir (`git branch --show-current` bu dal değilse önce `git switch $AUTO_INTEGRATION_BRANCH`). Planın Branch alanı: `bot/<FAZ>-<NN> (taban: $AUTO_INTEGRATION_BRANCH)`. Zincirleme taban kuralı (§4) uygulanmaz.
- **Önce kayıtları düzelt:** Entegrasyon dalına birleşmiş (`git merge-base --is-ancestor bot/<X> $AUTO_INTEGRATION_BRANCH`) ama `DOĞRULANDI` görünen planların Durum satırını, `plans/README.md` satırını ve `docs/STATUS.md` plan satırını `KAPANDI (<tarih>, $AUTO_INTEGRATION_BRANCH)` yap; bu değişiklikleri yeni planla aynı commit'e kat.
- **Faz sınırı durdurmaz** (`AUTO_TARGET_PHASE`, ör. `F5`): Bir fazın DeepSeek'in yapabileceği işleri bittiyse (kalanlar yalnızca istemci/insan testi gerektiriyorsa) o faz için `docs/templates/PHASE_REPORT.md`'den `docs/phase-reports/<FAZ>-taslak.md` yaz (yoksa), insan testlerini `docs/STATUS.md` **`## Proje sahibi testleri (bekleyen)`** bölümüne (yoksa oluştur; her madde: test kimliği, ne yapılacak, hangi bot hesabı/komut, beklenen sonuç) ekle ve **bir sonraki fazın ilk planını yaz**. Faz kabulü (`KABUL_EDILDI`) beklenmez; STATUS'a "kabul bekliyor, sonraki faza geçildi (gece modu)" not et. `docs/17`'deki ön koşul "önceki faz kabulü" bu modda yalnızca kodu/veriyi gerçekten gereken teslimatlar için aranır.
- **Hedef faz bitti:** `AUTO_TARGET_PHASE` fazının DeepSeek'in yapabileceği işleri de bittiyse plan **yazma**; o fazın rapor taslağını yaz, `plans/.auto-loop-done` dosyasına tek satır neden yaz (`printf '%s' "F5 tamamlandı: ..." > plans/.auto-loop-done`) ve kayıtları commit et.
- **İstemci/GUI gerektiren iş planlanmaz.** Sunucu tarafında çalıştırılabilen testler (sunucuyu `tools/run-servers.sh start` ile açmak, botları sunucu içinde spawn etmek, log/DB/paket izleyici ile ölçmek) plana konabilir; plan sonunda sunucuları kapatmayı (`tools/run-servers.sh stop`) şart koş. İnsan istemcisi gerektiren doğrulamalar "Proje sahibi testleri (bekleyen)" listesine gider; plan bunları beklemeden yalnızca kod/derleme/otomatik test ile tamamlanabilir olmalı.
- **Plan boyutu:** Gece planları küçük tutulur (≤ ~6 dosya, tek bir net yetenek); her plan sonunda proje derlenir ve bot sistemi kapalıyken (varsayılan) sunucu davranışı değişmez. Büyük işler birden çok sıralı plana bölünür (her seferinde yalnızca sıradakini yaz).
- **Karar yetkisi sende:** Tasarım kararlarını (ADR-0005..0008 dahil) önerilen seçenekle sen ver; ADR başlığına `(otonom döngüde Claude kararı — gözden geçirilmeli)` ekle.

`AUTO_LOOP` 1 değilse kullanıcı karşındadır: kararları ona **tek tek ve sade dille** sor (bölüm 5.3).

### 0.2 Paralel hat (`AUTO_TRACK` dolu)

`echo "$AUTO_TRACK | $AUTO_INTEGRATION_BRANCH | $AUTO_TRACK_TOPIC"` çalıştır. `AUTO_TRACK` doluysa (ör. `nav`) bu çağrı, ana hattan **bağımsız ikinci** döngüden gelir: ayrı bir git worktree'sinde, kendi entegrasyon dalında (`$AUTO_INTEGRATION_BRANCH`) çalışır. Ana hat F4 işlerini yapar; onun işlerini **yazma**. Bu modda §0 ve §0.1 şu farklarla uygulanır (bunlar önceliklidir):

- **Konu:** Planın konusu `AUTO_TRACK_TOPIC` metnindedir; `docs/STATUS.md` "Sıradaki adımlar"ından iş seçme. Konudaki sırayı izle: `plans/README.md`'de bu hattın planları (ör. `F5-NN`) varsa sıradaki dilimi seç; yoksa ilk dilimi yaz. Plan numarası o hattın fazındaki sıradaki `NN`'dir (ör. `F5-01`).
- **Faz sınırı yok sayılır:** Konu `docs/17` §1 "Paralel yürütülebilir işler" ile izinlidir; F4'ün bitmesini bekleme. STATUS'ta aktif faz F4 olsa da bu hatta F5 planı yaz. Faz rapor taslağı yazma.
- **Kapsam (sert):** Yalnızca sunucusuz saf mantık: `BotCore/` (başlık-yalnızca, `windows.h`/`stdafx.h`/`GameServer`/`shared` içermez; ADR-0016), `BotCoreTests` birim testleri, `tools/` (Python/betik) ve `docs/`. `GameServer/`, `AIServer/`, `shared/` **değiştirilmez**; sunucu çalıştırılmaz, DB'ye bağlanılmaz, istemci gerekmez. Sunucuya bağlama (GameServer entegrasyonu) bu hattın işi değildir; F4 bitince ayrı plan olur.
- **Sunucuya dokunma:** Plan `tools/run-servers.sh` çağırmayı şart koşmaz (ana hat sunucuyu kullanıyor olabilir). Doğrulama: `./tools/build.sh Release` (GameServer çözümünün hâlâ derlendiğini gösterir) ve `./tools/run-tests.sh`. Kabul kriterleri sayısal ve komutla doğrulanabilir olmalı (birim test adları, beklenen çıktı).
- **Ortak dosyalar:** `docs/STATUS.md` ve `plans/README.md` bu hatta kendi kopyandadır; ana hatla birleştirmeyi Claude/proje sahibi yapar. Yalnızca kendi planınla ilgili satırlara dokun, ana hattın satırlarını yeniden düzenleme.
- **Hedef bitti:** Konudaki tüm dilimler `KAPANDI`/`DOĞRULANDI` ise plan yazma; `plans/.auto-loop-done` dosyasına tek satır neden yaz ve kayıtları commit et.
- **Plan boyutu ve kalite aynı kalır** (§0.1): küçük, tek yetenek, ≤ ~6 dosya. Hız için plan büyütülmez.

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
- **Plan numarası:** `NN` o fazdaki **sıradaki serbest** numaradır; `50`–`59` aralığı değerlendirme düzeltme planlarına ayrılmıştır (`F4-50..59`, `F5-50..59`, `plans/README.md` "değerlendirme" tablosu) ve **atlanır**. Yani README'deki en yüksek numaradan değil, son *döngü* planından devam et (örn. `F4-24`'ten sonra `F4-25`; `F4-50..54` zaten yazılmıştır, onlar `plans/.queue` ile sırayla uygulanır, yeniden yazma).
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
