-- 004_bot_inventory_rollback.sql
-- Purpose: undo 004_bot_inventory.sql. Restores the 14..21 bytes of every bot
-- row saved in dbo.USERDATA_BOT_STOCK_BACKUP, then deletes those backup rows
-- (the table itself is kept), so a second run changes nothing (restored=0).
-- Only the 12 bot rows are touched; row contents are never printed.
--
-- Usage (game server MUST be stopped):
--   sqlcmd -S .\SQLEXPRESS -E -d FDP_kn_online -b -i db/004_bot_inventory_rollback.sql
--
-- When there is no backup table the script reports restored=0 instead of
-- failing (nothing to undo).

SET NOCOUNT ON;
SET XACT_ABORT ON;
GO

IF OBJECT_ID(N'dbo.USERDATA_BOT_STOCK_BACKUP', N'U') IS NULL
BEGIN
    PRINT 'BOTSTOCK_ROLLBACK: restored=0';
    RETURN;
END
GO

DECLARE @restored int = 0;

BEGIN TRANSACTION;

UPDATE u
SET u.strItem = CONVERT(binary(584), STUFF(u.strItem, 113, 64, b.OldSlots))
FROM dbo.USERDATA AS u
JOIN dbo.USERDATA_BOT_STOCK_BACKUP AS b ON b.strUserID = u.strUserID
JOIN (VALUES
    ('BotWP_K'), ('BotWG_K'), ('BotPHD_K'), ('BotPHB_K'), ('BotMF_K'), ('BotMI_K'),
    ('BotWP_E'), ('BotWG_E'), ('BotPHD_E'), ('BotPHB_E'), ('BotMF_E'), ('BotMI_E')
) AS bots(charName) ON bots.charName = u.strUserID;

SET @restored = @@ROWCOUNT;

-- Drop the backup rows just undone so the rollback is idempotent; the table stays.
DELETE b
FROM dbo.USERDATA_BOT_STOCK_BACKUP AS b
JOIN (VALUES
    ('BotWP_K'), ('BotWG_K'), ('BotPHD_K'), ('BotPHB_K'), ('BotMF_K'), ('BotMI_K'),
    ('BotWP_E'), ('BotWG_E'), ('BotPHD_E'), ('BotPHB_E'), ('BotMF_E'), ('BotMI_E')
) AS bots(charName) ON bots.charName = b.strUserID;

COMMIT TRANSACTION;

PRINT 'BOTSTOCK_ROLLBACK: restored=' + CONVERT(varchar(10), @restored);
GO
