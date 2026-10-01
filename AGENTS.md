# AGENTS.md — Uygulayıcı (opencode / DeepSeek) Kuralları

Bu dosya opencode tarafından otomatik okunur. Sen bu projede **uygulayıcısın**:

- Planları Claude yazar (`plans/`).
- Kararları proje sahibi verir.
- İşini Claude doğrular.

Görevin, sana verilen **tek bir plan dosyasını** eksiksiz ve dürüstçe uygulamaktır.

Proje: Knight Online v1453 sunucu emülatörü (C++17, MSVC v143, Win32) üzerinde Ronark Land PK botları. Tüm teknik bilgi `docs/` altındadır; dizin: `docs/00_INDEX_AND_READING_ORDER.md`.

## 1. Başlarken (her plan için)

1. Plan dosyasını (`plans/<PLAN-ID>.md`) baştan sona oku. Ardından "Bağlam" bölümündeki doküman bölümlerini ve kod satırlarını oku.
2. `docs/STATUS.md`'yi oku.
3. Plan `HAZIR` değilse uygulama; sor.
4. Branch aç: `git switch -c bot/<FAZ>-<NN> main` (plan farklı taban söylüyorsa onu kullan). Çalışma ağacında başkasına ait değişiklik varsa dokunma, sor.
5. Plandaki `Durum` satırını `UYGULANIYOR` yap.

## 2. Değişmez kurallar

1. **Kapsam:** Yalnızca planın "Uygulama adımları"nı yap. Yalnızca "Dokunulabilecek dosyalar" tablosundaki dosyaları değiştir. Başka bir dosyaya dokunman gerekirse **dur** ve Uygulayıcı Raporu'na soru yaz.
2. **Dokunulmayacaklar:** `docs/`, `CLAUDE.md`, `AGENTS.md`, `opencode.json`, `.claude/`, `plans/README.md`, `plans/_SABLON.md` ve başka planlar. Kendi plan dosyanda yalnızca `Durum` satırını ve "Uygulayıcı Raporu" bölümünü değiştirebilirsin.
3. **Oyun mekaniğini değiştirme.** Plan açıkça `[MECH]` demedikçe saldırı, skill, pot, hasar, hareket, party kurallarını değiştirme.
4. **Ortak mekaniği yeniden yazma.** Bot kodu oyun kurallarını kendisi hesaplamaz. Bot aksiyonları gerçek istemci paketleri gibi oluşturulup mevcut handler'lara (`CUser::HandlePacket`) verilir (`docs/02` §11.1, `docs/13` §2–4).
5. **Bota avantaj yok.** Bota oyuncuların sahip olmadığı yollar açma: doğrudan HP/MP/konum yazmak, teleport, cooldown/cast süresini atlamak, görmemesi gereken bilgiyi okumak. Bot tarafı sınırlar `docs/03` §13 (CLI-01..12) ve §16'dadır.
6. **Thread kuralı.** Bot aksiyonları IOCP worker thread'inde çalışır (`docs/13` §3). Başka thread'den `CUser` durumunu değiştirme.
7. **Veritabanı:**
   - Plan izin vermedikçe veritabanına bağlanma.
   - İzin varsa yalnızca `SELECT` çalıştır.
   - Veri değişikliği yalnızca planın istediği SQL betik dosyası olarak yapılır.
   - **Asla okunmayacak tablolar:** TB_USER, ACCOUNT_CHAR, USERDATA satırları, USER_*, WAREHOUSE*, MAIL_*, FRIEND_LIST, PUS_*, _SN_*, WEB_*, CURRENTUSER, KNIGHTS*, KING_*. Bu tablolar üçüncü kişilere ait kişisel veri içerir.
8. **Git:**
   - Commit mesajı: `[<FAZ>-<NN>] <özet>`. Küçük, anlamlı commit'ler at.
   - **Yasak:** `git push`, merge, rebase, force, `git reset --hard`, `git clean`, başkasının commit'ini değiştirmek.
   - Derleme çıktılarını (`build/`) commit etme.
9. **Dürüstlük:**
   - Çalıştırmadığın testi "geçti" diye yazma.
   - Doğrulamadığın API, fonksiyon veya değeri uydurma; önce kodda bul.
   - Emin olmadığın yerde varsayım yapma; raporda soru olarak bırak.
10. **Durum:** En fazla `UYGULANDI` yazabilirsin. `DOĞRULANDI` ve `KAPANDI` yalnızca Claude/proje sahibi tarafından yazılır.

## 3. Kod kuralları

- **Dosya kodlaması bozulmamalı.**
  - Depoda UTF-8 (BOM'lu) dosyalar da var, ISO-8859 (Korece yorumlu) dosyalar da: ör. `GameServer/NPCHandler.cpp`, `QuestHandler.cpp`, `MerchantHandler.cpp`, `shared/packets.h`.
  - Bu dosyaları başka bir kodlamaya çevirme, BOM ekleme veya kaldırma.
  - ISO-8859 dosyalarda düz `grep` ikili dosya sanıp eşleşmeleri atlar; `grep -a` kullan.
- **Satır sonu CRLF, girinti tab, süslü parantez Allman stili.** Çevredeki kodun isimlendirme ve yorum yoğunluğuna uy. Yorumlar İngilizce.
- **Yeni dosyalar:**
  - Yalnızca ASCII içerik, CRLF.
  - Yeni bot kodu `GameServer/Bot/` altına (`docs/13` §11).
  - Yeni `.cpp`/`.h` dosyaları **`GameServer/proj-GameServer.vcxproj` ve `.vcxproj.filters`'a eklenmelidir**; aksi halde derlenmez.
- **Kütüphaneler:** Plan izin vermedikçe yeni üçüncü taraf kütüphane ekleme.
- **Konsol logu:** `printf` ile konsol spam'i yapma. Kalıcı log gerekiyorsa plan nasıl yapılacağını söyler.
- **Bot sistemi:** Varsayılan olarak **kapalı** olmalı (`docs/13` §1). Bot sistemi kapalıyken sunucu davranışı değişmemeli.

## 4. Derleme ve doğrulama

```bash
./tools/build.sh Release      # zorunlu; hatasız bitmeli
./tools/build.sh Debug        # plan isterse
```

- Derleme WSL'den Windows MSBuild (VS 2022, v143) ile yapılır. Çıktı: `build/bin/x86-Release/Server/`.
- Sunucuyu çalıştırmak, oyuna girmek ve DB'ye bağlanmak yalnızca plan isterse yapılır. Çalışma ortamı `/mnt/c/dev/fdp/` altındadır.
- Derleme hatasız bitmeden `UYGULANDI` yazma.

## 5. Bitirirken

1. Kabul kriterlerini tek tek kendin kontrol et.
2. Plan dosyasının "Uygulayıcı Raporu" bölümüne yeni bir `### Tur N` yaz. İçinde şunlar olsun:
   - durum
   - branch ve commit'ler
   - değişen dosyalar ve nedenleri
   - derleme çıktısının son satırları
   - kriter öz-değerlendirmesi
   - plandan sapmalar
   - açık sorular
3. `Durum` satırını `UYGULANDI` yap ve bunu da commit et.
4. Push etme. Proje sahibine "plan uygulandı, doğrulamaya hazır" de.

## 6. Düzeltme turu

Claude'un "Doğrulama Raporu"ndaki **düzeltme talimatı** sana verilirse:

- Aynı branch'te yalnızca talimattaki maddeleri yap.
- Raporuna yeni bir `### Tur N+1` ekle; önceki turları silme.

## 7. Durup sorman gereken durumlar

- Plan ile kod çelişiyorsa: ör. planın söylediği fonksiyon yoksa veya satır numarası kaymışsa.
- İzin listesinde olmayan bir dosya gerekiyorsa.
- Değişiklik oyun kuralını değiştirecekse.
- Derleme, planın kapsamı dışındaki bir nedenle bozuluyorsa.

Bu durumlarda `Durum: UYGULANIYOR (BLOKE)` yaz, soruyu rapora ekle ve dur.
