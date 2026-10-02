-- 003_bot_quests_rollback.sql
-- Purpose: undo 003_bot_quests.sql. Restores sQuestCount, strQuest and (when
-- the run used QuestTestPoints=1) strSkill from dbo.USERDATA_BOT_QUEST_BACKUP.
-- Only the rows saved by the apply script are restored; the restored backup
-- rows are then removed, so a second run changes nothing (restored=0). The
-- backup table itself is kept.
--
-- Usage (game server MUST be stopped):
--   sqlcmd -S .\SQLEXPRESS -E -d FDP_kn_online -b -i db/003_bot_quests_rollback.sql
--
-- When there is no backup table the script reports restored=0 instead of
-- failing (nothing to undo).

SET NOCOUNT ON;
SET XACT_ABORT ON;
GO

IF OBJECT_ID(N'dbo.USERDATA_BOT_QUEST_BACKUP', N'U') IS NULL
BEGIN
    PRINT 'BOTQUEST_ROLLBACK: restored=0';
    RETURN;
END
GO

DECLARE @restored int = 0;

BEGIN TRANSACTION;

UPDATE u
SET u.sQuestCount = b.OldQuestCount,
    u.strQuest     = b.OldQuest,
    u.strSkill     = ISNULL(CONVERT(varchar(10), b.OldSkill), u.strSkill)
FROM dbo.USERDATA AS u
JOIN dbo.USERDATA_BOT_QUEST_BACKUP AS b ON b.strUserID = u.strUserID;

SET @restored = @@ROWCOUNT;

-- Drop the backup rows just undone so the rollback is idempotent; the table stays.
DELETE b
FROM dbo.USERDATA_BOT_QUEST_BACKUP AS b
JOIN dbo.USERDATA AS u ON u.strUserID = b.strUserID;

COMMIT TRANSACTION;

PRINT 'BOTQUEST_ROLLBACK: restored=' + CONVERT(varchar(10), @restored);
GO
