import { StopCircle } from "lucide-react";
import { ConfirmActionDialog } from "../shared/confirm-action";

export function StopServerButton({ name }: { name: string }) {
  return (
    <ConfirmActionDialog
      title="Stop server?"
      description={`"${name}" will be shut down and all active sessions will be terminated.`}
      confirmLabel="Stop"
      onConfirm={() => console.log("stop", name)}
    >
      <button className="flex items-center gap-2 rounded px-2 py-1.5 text-xs text-red-400 hover:bg-red-500/10 hover:text-red-300 w-full">
        <StopCircle className="size-3.5" />
        Stop
      </button>
    </ConfirmActionDialog>
  );
}
