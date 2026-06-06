import { Info, Network, PlugIcon, Shell, StarsIcon } from "lucide-react";
import { cn } from "../../lib/utils";

const navItems = [
  { label: "Interactive", icon: Shell },
  { label: "Info", icon: Info },
  { label: "Plugins", icon: PlugIcon },
  { label: "Network", icon: Network },
  { label: "AI", icon: StarsIcon },
];

export function Tab({
  activeTab,
  setTab,
}: {
  activeTab: string;
  setTab: (t: string) => void;
}) {
  return (
    <div className="flex border-b border-zinc-800">
      {navItems.map(({ label, icon: Icon }) => (
        <button
          key={label}
          onClick={() => setTab(label)}
          className={cn(
            "px-8 py-4 w-full text-sm font-mono uppercase tracking-widest transition-all border-b-2",
            activeTab === label
              ? "text-white border-emerald-500"
              : "text-zinc-500 hover:text-zinc-400 border-transparent",
          )}
        >
          <Icon className="inline-block mr-2 w-4 h-4" />
          {label}
        </button>
      ))}
    </div>
  );
}
