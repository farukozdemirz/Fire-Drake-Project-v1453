-- 002_bot_characters_rollback.sql
-- Purpose: remove the 12 bot characters created by 002_bot_characters.sql.
-- Usage:
--   sqlcmd -S .\SQLEXPRESS -E -d FDP_kn_online -b -i db/002_bot_characters_rollback.sql
--
-- Ownership check: a bot name/account is only removed when the matching
-- BotAcc<PROFILE><K|E> account lists that character as strCharID1. Otherwise the script
-- raises an error and deletes nothing (the row may belong to a real player).
-- Only the 12 bot accounts/characters are touched (USERDATA, ACCOUNT_CHAR,
-- WAREHOUSE); personal data tables are never read or written.

SET NOCOUNT ON;
SET XACT_ABORT ON;
GO

BEGIN TRANSACTION;

DECLARE @bots TABLE (charName varchar(21), account varchar(21));
INSERT INTO @bots (charName, account) VALUES
    ('BotWP_K','BotAccWPK'), ('BotWG_K','BotAccWGK'),
    ('BotPHD_K','BotAccPHDK'), ('BotPHB_K','BotAccPHBK'),
    ('BotMF_K','BotAccMFK'), ('BotMI_K','BotAccMIK'),
    ('BotWP_E','BotAccWPE'), ('BotWG_E','BotAccWGE'),
    ('BotPHD_E','BotAccPHDE'), ('BotPHB_E','BotAccPHBE'),
    ('BotMF_E','BotAccMFE'), ('BotMI_E','BotAccMIE');

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
