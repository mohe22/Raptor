import { Square } from "lucide-react";
import { ConfirmActionDialog } from "../shared/confirm-action";

export function PauseServerButton({ name }: { name: string }) {
  return (
    <ConfirmActionDialog
      title="Pause server?"
      description={`"${name}" will stop accepting new connections but keep existing sessions alive.`}
      confirmLabel="Pause"
      onConfirm={() => console.log("pause", name)}
    >
      <div className="flex items-center gap-1 border h-6 px-1.5 text-xs transition border-red-500/30 text-red-400 hover:bg-red-500/10">
        <Square className="h-3 w-3" />
        pause
      </div>
    </ConfirmActionDialog>
  );
}
