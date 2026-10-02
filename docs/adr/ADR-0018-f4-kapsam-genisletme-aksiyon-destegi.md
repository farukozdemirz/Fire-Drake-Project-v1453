# ADR-0018: F4 kapsam genişletmesi: aksiyon desteğinin tamamlanması (otonom döngüde Claude kararı — gözden geçirilmeli)

Durum: KABUL (geçici, proje sahibi gözden geçirecek) · Tarih: 2026-10-02 · Karar veren: Claude (proje sahibi "eksiksiz ama hızlı" dedi; ana hat F4-23'te "F4 tamamlandı" diyerek durdu)
İlgili: `docs/17` F4 (§2), `docs/phase-reports/F4-taslak.md` §3, ADR-0017 (Ek F4-03: cast sınırı, Ek F4-04: pot, Ek F4-16/18: algı), `docs/05` (skill kataloğu), `docs/03` CLI-03/07/09/12

## Bağlam
F4-01..F4-23 KAPANDI ve `docs/17` F4 listesindeki dilimler bitti, ama faz taslağı (§3) şunları "ertelenen" olarak bırakıyor: uçan, çift tipli (`bType[1] != 0`, buz büyüleri), Type4 (buff/debuff), alan ve eşya tüketen skill'ler `unsupported_skill`; cast iptali ve hareketle iptal yok; CLI-07 (alan hedef noktası) ve CLI-12 (speedhack kontrol paketi) kodda yok; pot/skill tüketimi için envanter doldurma yok; T-MECH-SKILL'in bot tarafından yeniden koşulması buna bağlı ertelenmiş; başkalarının oturma bayrağı izlenmiyor. 2026-10-02 testleri bunu doğruladı: mage'in ana skill'leri (Fire ball, Prismatic) ve priest'in Malice'i botla atılamıyor. Warrior/priest/mage davranışları (F6+) bu aksiyonlar olmadan yazılamaz.

## Karar
Bu işler **F4'ün parçası** sayılır (aksiyon yürütme = "botların temel aksiyonlarını yapabilmesi", `docs/17` F4 amacı). F4, aşağıdaki dilimler bitmeden "tamamlandı" ilan edilmez. Her dilim küçük bir plandır (≤ ~6 dosya, tek yetenek), aynı doğrulama eşiğiyle (derleme + birim test + çalışma zamanı) ilerler. Sıra (plan-olustur her seferinde yalnızca sıradakini yazar; bir dilim gerekirse bölünür):
1. Cast iptali (`MAGIC_FAIL -100`), hareketle iptal, `UseStanding` otomatik durdurma (CLI-03/CLI-09 tam).
2. Uçan skill'ler (`bType[0] == 2` / `FlyingEffect != 0`: Fire ball, Ice arrow, Fire spear...).
3. Çift tipli Type3 skill'ler (`bType[1] != 0`: buz büyüleri, Prismatic).
4. Type4 buff/debuff skill'leri (Malice, AC/HP buff'ları; kendine/dosta/düşmana) ve `BuffView` ile tutarlılık.
5. Alan skill'leri (`bMoral` 10..13, CLI-07 hedef noktası).
6. Cure/diriltme/summon gibi Type5+ ve eşya tüketen (`UseItem`) skill'ler için ilk dilim(ler): hangi tiplerin gerçekten gerektiği `docs/05`/`docs/06..08` ihtiyaçlarına göre seçilir.
7. CLI-12 (`WIZ_SPEEDHACK_CHECK` yanıtı).
8. Pot ve skill tüketimi için envanter doldurma/yeniden stoklama (`ScenarioRunner` ile).
9. T-MECH-SKILL'in botla betikli yeniden koşusu (warrior/priest/mage).
10. Algı eksikleri: başkalarının oturma bayrağı (`WIZ_STATE_CHANGE`), `PARTY_LEVELCHANGE`/`STATUSCHANGE`.

## Sonuçlar
- F4 kabulü bu dilimlerin KAPANDI olmasına ve faz taslağının (§3) güncellenmesine bağlanır.
- Hangi skill tipinin hangi davranış fazında zorunlu olduğu zinciri (davranış → skill → aksiyon desteği → algı → oyun içi kabul) `docs/` değerlendirme çalışmasında ayrıca işlenir; çakışırsa bu ADR'nin sırası kalır, kapsam daraltılabilir ama genişletilmez.
- Faz kabulü (`KABUL_EDILDI`) yine yalnızca proje sahibinindir.
