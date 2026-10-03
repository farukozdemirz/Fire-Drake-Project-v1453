# ADR-0002: Level 80 master karakterlerin oluşturulması

Durum: KABUL · Tarih: 2026-10-01 · Karar veren: proje sahibi (oturumda tek tek soru-cevap ile)
İlgili: K-2, K-3, REQ-NEW-04, F1–F2

## Bağlam
Bu kurulumda oyun içi sınıf yükseltme akışı yok; başlangıç sınıfı skill puanı dağıtamıyor; master skill'ler sınıf koduna birebir bağlı (MEC-CHR-01..03) `[D]`/`[V]`.

## Karar
Bot karakterleri sunucu kapalıyken bir DB kurulum betiğiyle oluşturulur: level 80, master sınıf kodu, stat (toplam 577, ≤ 255), skill puanları (142; ağaç ≤ 80, master ≤ 20), `strItem` (referans set), zone 71, NP > 0, WAREHOUSE satırı. Betik değişmezleri doğrular (T-DATA-01). **İnsan test hesapları da aynı betikle, aynı stat dağılımı ve aynı referans setle hazırlanır (K-3).**

## Değerlendirilen alternatifler
| Alternatif | Artılar | Eksiler | Neden seçilmedi |
|---|---|---|---|
| Oyuna sınıf atlama eklemek | Gerçekçi | Bot işinden bağımsız ek geliştirme | Kapsam |
| Yeni GM komutu | Sunucu açıkken ayar | Sunucu kodu değişir; tekrarlanabilirlik düşük | Tekrarlanabilirlik |

## Sonuçlar
Karakter verisi betikte tek kaynaktan gelir; test tekrarlanabilir. Girişte sunucu bu alanları doğrulamadığı için doğrulama betiğin sorumluluğundadır.

## Doğrulama
T-DATA-01, T-DATA-03; insan değerlendirme oturumunda karakterlerin set uyumu.

## Ek F8-02: Kompozisyon/karakter seti hesaplayıcısının kuralları (otonom döngüde Claude kararı — gözden geçirilmeli)
Tarih: 2026-10-03 · Plan: `plans/F8-02-bot-kompozisyon-denetleyici.md` · Dayanak: `docs/09` §2.3/§2.4 (kompozisyonlar), `docs/15` §6a (16/20 karakter dökümü), `docs/04` §3.3, `db/002_bot_characters.sql` (satır biçimi), REQ-PTY-02.

1. **Araç yalnızca okur `[Ö]`:** `tools/bot-composition-check.py`, `db/002` (ve ileride `db/005`, `db/006`) SQL dosyasındaki `@bots` satırlarını metin olarak ayrıştırır; DB'ye bağlanmaz, SQL çalıştırmaz. Karakter varlığının kaynağı SQL betiğidir (bu ADR "Sonuçlar": tek kaynak).
2. **Ulus başına ihtiyaç = kompozisyonların profil başına en büyüğü `[A]`:** `docs/15` §6a "3. W-P ve 3. M-F aynı sabit karakter kümesinden seçilebilsin" der; hangi ulusun hangi kompozisyonu oynayacağı (EVAL-8v8-MIX: C8-B vs C8-C) tanımsız olduğundan iki ulus da aynı (en büyük) kümeyi taşır. Sonuç: `small` (C2..C5) 5/ulus = 10, `min16` (C8-A) 8/ulus = 16, `full20` (C8-A..D) 10/ulus = 20; bugünkü 12 karakter için eksik 0 / 4 / 8. `docs/15` §6a'nın "20 = 16 + 4" ifadesiyle uyumludur (16'ya göre +4).
3. **Ek karakter adlandırması `[A]` (db/005 planının girdisi):** indeks 1 mevcut adı korur (`BotWP_K`, hesap `BotAccWPK`); indeks n ≥ 2: karakter `Bot<PROFİL><n>_<K|E>` (`BotWP2_K`), hesap `BotAcc<PROFİL><n><K|E>` (`BotAccWP2K`). Hesap adı yalnızca harf/rakam (`docs/04` §3.4), karakter adı ≤ `MAX_ID_SIZE` 20 (`shared/globals.h:12`). Profil kodu satırda aynı kalır (`'WP'`), yani `db/005`/`db/006` aynı satır biçimini kullanır; farklı biçim seçilirse araç genişletilir.
4. **Kompozisyon kuralı:** yalnızca REQ-PTY-02 (≤ 2 priest) sert denetimdir; CMP-01..04 öneri olduğundan denetlenmez. EVAL-1v1/5v8/HEALSTALL/WIPE (kompozisyonu tanımsız) kapsam dışıdır.
5. **Bilinçli ertelenen:** `db/005`/`db/006` yazımı (F8-03/F8-04; ayrı plan; sunucu kapalıyken DB'ye yazma), `BOT_TABLE`'ın SQL/ini kaynaklı olması, 32/64 bot performans kümesi (`docs/15` §6a son cümle), ırk (`Race`) denetimi.
