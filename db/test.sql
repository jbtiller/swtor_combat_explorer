SELECT DISTINCT c.ts_begin, an.name, dn.name, lf.filename, pc.name FROM Event
    JOIN Combat as c  ON Event.combat = c.id
    JOIN Area   as ar ON c.area = ar.id
      JOIN Name as an ON ar.area = an.id
      JOIN Name as dn ON ar.difficulty = dn.id
    JOIN Log_File as lf ON c.logfile = lf.id
    JOIN Actor    as pc ON Event.source = pc.id OR Event.target = pc.id
  GROUP BY c.ts_begin
  ORDER BY an.name, dn.name, pc.name;
