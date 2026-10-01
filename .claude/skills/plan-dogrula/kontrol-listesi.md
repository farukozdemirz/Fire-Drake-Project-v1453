# Doğrulama kontrol listesi

Her madde için sonuç: ✔ (kanıtıyla), ✘ (bulgu) veya — (bu plana uygulanmaz).

## A. Kapsam ve git

- [ ] Branch adı `bot/<FAZ>-<NN>`, taban `main` (veya planın belirttiği taban).
- [ ] Commit mesajları `[<FAZ>-<NN>] …` biçiminde. Merge/rebase/force izi yok. `build/` çıktısı commit edilmemiş.
- [ ] `git diff --stat` içindeki her dosya planın "Dokunulabilecek dosyalar" listesinde.
- [ ] `docs/`, `CLAUDE.md`, `AGENTS.md`, `opencode.json`, `.claude/`, başka planlar değişmemiş. Kendi planında yalnızca `Durum` ve Uygulayıcı Raporu değişmiş: `git diff main...bot/<…> -- plans/`.
- [ ] Kapsam dışı iş yok: ilgisiz yeniden düzenleme, toplu biçimlendirme, silinen yorum/kod, "bu arada düzelttim" değişikliği.

## B. Dosya biçimi

- [ ] Değişen dosyaların kodlaması korunmuş. `git diff` ile karşılaştır, `file` çıktısı öncesi/sonrası aynı olmalı. ISO-8859 dosyalarda Korece yorum satırları bozulmamış (`grep -a`). BOM eklenmemiş/silinmemiş.
- [ ] Satır sonları CRLF (`git diff` içinde tüm dosyada `^M` değişimi görünmüyor). Girinti tab, Allman süslü parantez.
- [ ] Yeni dosyalar ASCII, CRLF. Yeni `.cpp`/`.h` dosyaları `proj-*.vcxproj` ve `.filters`'a eklenmiş.

## C. Derleme

- [ ] `./tools/build.sh Release` hatasız. Plan istiyorsa `Debug` da hatasız.
- [ ] Değişen dosyalarda yeni derleyici uyarısı yok (ya da gerekçeli).

## D. Proje kuralları

- [ ] **Mekanik değişmedi** (plan `[MECH]` değilse): `AttackHandler.cpp`, `MagicInstance.cpp`, `MagicProcess.cpp`, `Unit.cpp` hasar/kural kodu, `CharacterMovementHandler.cpp` doğrulamaları, `PartyHandler.cpp` kuralları dokunulmamış ya da yalnızca plandaki kanca eklenmiş.
- [ ] **Ortak mekanik yeniden yazılmadı:** bot kodu hasar, cooldown, menzil kuralını kendi hesaplayıp uygulamıyor. Aksiyonlar paket olarak `HandlePacket` yolundan geçiyor (`docs/02` §11.1). Bot tarafındaki ön kontroller yalnızca "gereksiz deneme"yi önlemek için.
- [ ] **Bota avantaj yok:** doğrudan `m_sHp`/`m_sMp`/konum yazımı, `Warp`/teleport, cooldown haritalarına müdahale, gözlem sözleşmesi dışı bilgi okuma (`docs/03` §16) yok. İstisna yalnızca planda açıkça "test modu" diye tanımlanmış olanlar.
- [ ] **Adalet sınırları** (plan bot aksiyonu içeriyorsa): CLI-01..12'den ilgili olanlar uygulanmış. Cast süresi beklenmiş, aynı saniye tekrarları engellenmiş.
- [ ] **Thread:** `CUser`/`Unit` durumu yalnızca IOCP worker thread'inde değişiyor (`docs/13` §3). Yeni kilitler mevcut kilit sırasıyla çakışmıyor (`C3DMap::m_lock` → `CRegion::m_lock`; `KOSocketMgr::GetLock()`).
- [ ] **Bot sistemi varsayılan kapalı;** kapalıyken mevcut kod yolları değişmemiş. Ör. gerçek oyuncu oturumlarında `m_botSink == nullptr`.
- [ ] **DB:** yasak tablolara erişim yok. Veri değişikliği yalnızca plandaki SQL betiğinde ve geri alma adımıyla.
- [ ] Gizli bilgi (parola, anahtar, kişisel veri) commit edilmemiş.

## E. Kod kalitesi

- [ ] Doğruluk: null/sınır kontrolleri, tamsayı taşması, bellek/oturum sızıntısı, hata yolları.
- [ ] Çevredeki kodun isimlendirme ve yorum yoğunluğuna uyum; gereksiz soyutlama yok.
- [ ] Ölü kod, debug `printf`, yorum satırına alınmış kod bırakılmamış.

## F. Kabul kriterleri ve rapor

- [ ] Planın her kabul kriteri ayrı ayrı, kanıtla doğrulandı (tabloya işlenir).
- [ ] Uygulayıcı Raporu'ndaki iddialar doğru: commit listesi, değişen dosyalar, derleme çıktısı.
- [ ] Uygulayıcının sapma ve sorularına cevap verildi. Gerekirse kullanıcıya karar olarak soruldu.
