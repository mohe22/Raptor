function Seg({
  icon: Icon,
  children,
}: {
  icon: React.ElementType;
  children: React.ReactNode;
}) {
  return (
    <div className="flex items-center gap-1.5 px-3 py-2 border-r border-border last:border-r-0 whitespace-nowrap">
      <Icon className="w-3.5 h-3.5  text-primary shrink-0" />
      <span className="text-sm text-foreground">{children}</span>
    </div>
  );
}
export function AgentStatusBar({
  data,
  config,
}: {
  data: Sess;
  config: OSConfig;
}) {
  return (
    <div
      style={{ scrollbarWidth: "none", msOverflowStyle: "none" }}
      className="flex items-stretch border-b overflow-hidden bg-background overflow-x-auto"
    >
      <div className="flex items-center gap-1.5 px-3 py-2 border-r border-border last:border-r-0 whitespace-nowrap">
        <span className="text-md">{config.emoji}</span>
        <span className="text-sm text-foreground">{data.host}</span>
      </div>
      <Seg icon={Cpu}>
        {data.os} {data.osV}
        <span className="text-xs text-muted-foreground ml-1">{data.arch}</span>
      </Seg>

      <Seg icon={Server}>
        <Link className=" hover:underline" to={`/servers/${data.connectedTo}`}>
          {data.connectedTo}
        </Link>
      </Seg>

      <Seg icon={User}>
        {data.username}
        {data.isAdmin && (
          <span className="ml-1.5 text-xs font-medium px-1.5 py-0.5 rounded-full bg-emerald-100 text-emerald-800 dark:bg-emerald-950 dark:text-emerald-300">
            admin
          </span>
        )}
      </Seg>

      <Seg icon={Globe}>{data.country}</Seg>

      <Seg icon={Network}>{data.primaryIP}</Seg>

      <Seg icon={Cpu}>
        {data.cpuCores} cores
        <span className="text-xs text-muted-foreground ml-1">
          {data.cpuUsage}%
        </span>
      </Seg>

      <Seg icon={HardDrive}>
        {formatGB(data.usedRam)} / {formatGB(data.totalRam)}
      </Seg>

      <Seg`y icon={Terminal}>
        <span className="text-muted-foreground text-xs">PID</span>
        <span className="ml-1">{data.processID}</span>
      </Seg>

      <Seg icon={Clock}>{formatUptime(data.uptime)}</Seg>

      {data.isAdmin && (
        <Seg icon={ShieldCheck}>
          <span className="text-xs font-medium text-emerald-600 dark:text-emerald-400">
            elevated
          </span>
        </Seg>
      )}

      {data.isVM && (
        <Seg icon={Box}>
          <span className="text-xs font-medium px-1.5 py-0.5 rounded-full bg-blue-100 text-blue-800 dark:bg-blue-950 dark:text-blue-300">
            VM
          </span>
        </Seg>
      )}
    </div>
  );
}
