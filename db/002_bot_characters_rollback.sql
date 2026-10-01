-- 002_bot_characters_rollback.sql
-- Purpose: remove the 12 bot characters created by 002_bot_characters.sql.
-- Usage:
--   sqlcmd -S .\SQLEXPRESS -E -d FDP_kn_online -b -i db/002_bot_characters_rollback.sql
--
-- Ownership check: a bot name/account is only removed when the matching
-- BotAcc_... account lists that character as strCharID1. Otherwise the script
-- raises an error and deletes nothing (the row may belong to a real player).
-- Only the 12 bot accounts/characters are touched (USERDATA, ACCOUNT_CHAR,
-- WAREHOUSE); personal data tables are never read or written.

SET NOCOUNT ON;
SET XACT_ABORT ON;
GO

BEGIN TRANSACTION;

DECLARE @bots TABLE (charName varchar(21), account varchar(21));
INSERT INTO @bots (charName, account) VALUES
    ('BotWP_K','BotAcc_WP_K'), ('BotWG_K','BotAcc_WG_K'),
    ('BotPHD_K','BotAcc_PHD_K'), ('BotPHB_K','BotAcc_PHB_K'),
    ('BotMF_K','BotAcc_MF_K'), ('BotMI_K','BotAcc_MI_K'),
    ('BotWP_E','BotAcc_WP_E'), ('BotWG_E','BotAcc_WG_E'),
    ('BotPHD_E','BotAcc_PHD_E'), ('BotPHB_E','BotAcc_PHB_E'),
    ('BotMF_E','BotAcc_MF_E'), ('BotMI_E','BotAcc_MI_E');

-- A character row may only be removed when its expected bot account owns it.
IF EXISTS (
    SELECT 1
    FROM @bots b
    WHERE EXISTS (SELECT 1 FROM dbo.USERDATA u WHERE u.strUserID = b.charName)
      AND NOT EXISTS (
          SELECT 1 FROM dbo.ACCOUNT_CHAR a
          WHERE a.strAccountID = b.account AND a.strCharID1 = b.charName)
)
BEGIN
    RAISERROR(N'refusing to delete: a bot character name is not owned by its expected account', 16, 1);
    ROLLBACK TRANSACTION;
    RETURN;
END

-- A bot account row may only be removed when it lists our bot character.
IF EXISTS (
    SELECT 1
    FROM @bots b
    WHERE EXISTS (SELECT 1 FROM dbo.ACCOUNT_CHAR a WHERE a.strAccountID = b.account)
      AND NOT EXISTS (
          SELECT 1 FROM dbo.ACCOUNT_CHAR a
          WHERE a.strAccountID = b.account AND a.strCharID1 = b.charName)
)
BEGIN
    RAISERROR(N'refusing to delete: a bot account exists without the expected character', 16, 1);
    ROLLBACK TRANSACTION;
    RETURN;
END

DELETE FROM dbo.WAREHOUSE WHERE strAccountID IN (SELECT account FROM @bots);
DELETE FROM dbo.USERDATA WHERE strUserID IN (SELECT charName FROM @bots);
DELETE FROM dbo.ACCOUNT_CHAR WHERE strAccountID IN (SELECT account FROM @bots)
    AND strCharID1 IN (SELECT charName FROM @bots);

COMMIT TRANSACTION;

SELECT
    (SELECT COUNT(*) FROM dbo.USERDATA WHERE strUserID IN (SELECT charName FROM @bots)) AS remaining_bot_chars,
    (SELECT COUNT(*) FROM dbo.ACCOUNT_CHAR WHERE strAccountID IN (SELECT account FROM @bots)) AS remaining_bot_accounts,
    (SELECT COUNT(*) FROM dbo.WAREHOUSE WHERE strAccountID IN (SELECT account FROM @bots)) AS remaining_bot_warehouses;
GO
