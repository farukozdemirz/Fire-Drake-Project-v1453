# ADR-0025: Debug testleri plan bazında yalnızca eklenen/değişen testlerle koşulur

Durum: KABUL
Tarih: 2026-10-03 · Karar veren: proje sahibi ("Debug'da tüm paketi koşmak yerine yalnızca planın eklediği veya değiştirdiği testleri koşmak doğru yaklaşım; diğer şekilde ilerlemek bir hata")
İlgili: `plans/OTONOM_DONGU.md` §13-14, `tools/run-changed-tests.sh`, `.claude/skills/plan-dogrula`, `.claude/skills/plan-olustur`, `AGENTS.md` §4

## Bağlam

Doğrulama süresinin büyük kısmı Debug yapılandırmasının tam test paketidir. Ölçüm (hat `f6`, F6-02, 341 test): Release tam paket yaklaşık 35 sn, Debug tam paket yaklaşık 10 dk (`plans/_logs/evidence/` kaydı). Debug paketi hem uygulayıcıda hem denetçide koşuluyor; kanıt adımı denemesinde (aynı gün) Debug iki kez koşulup doğrulama 32 dk sürdü (eski akışta benzer plan 15 dk). Debug'a özgü hatalar (assert, başlatılmamış bellek) nadirdir ve plan farkından bağımsız tam paketle ancak toplu yakalanır.

## Karar

1. **Release:** her planda tam paket aynen (`tools/run-tests.sh Release`, `0 failed`).
2. **Debug derleme:** `tools/build.sh Debug` hatasız ve yeni uyarısız (plan isterse; tam derleme ~20 sn).
3. **Debug testler:** yalnızca planın **eklediği veya değiştirdiği** testler: `tools/run-changed-tests.sh Debug <taban>` (taban = entegrasyon dalı; değişen test dosyalarındaki tüm `TEST_CASE`'ler + farkta eklenen `TEST_CASE`'ler). Planın kabul kriterinde "Debug tam paket 0 failed" yazıyorsa bu karar uyarınca yukarıdaki seçim olarak okunur; plan metninin değişmesi gerekmez.
4. **Tam Debug paketi** şu kapılarda koşulur: (a) `main`'e birleştirme kapısı (`./tools/build.sh Release` + `run-tests.sh Release` yanında `Debug` tam paket); (b) faz sonu raporu; (c) üretim koduna (`GameServer/`, `shared/`) dokunan planın doğrulamasında denetçi isterse. Kapı başarısız olursa son birleşmeler arasında hangisinin kırdığı bisect edilir.
5. Kapsam: yalnızca Debug test paketi. Release tam paket, Debug derleme, çalışma zamanı kanıtları ve negatif kontroller değişmez.

## Sonuçlar

- Her plan doğrulaması ve uygulaması yaklaşık 8-10 dk kısalır (Debug tam paket çıkar).
- Risk: Debug'a özgü hata plan bazında değil kapıda yakalanır; kabul edildi (bugüne kadar Debug'ın tek başına yakaladığı hata kaydı yok `[Ö]`).
