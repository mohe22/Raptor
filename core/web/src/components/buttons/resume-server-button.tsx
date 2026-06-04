import { Play } from "lucide-react";
import { ConfirmActionDialog } from "../shared/confirm-action";

export function ResumeServerButton({ name }: { name: string }) {
  return (
    <ConfirmActionDialog
      title="Resume server?"
      description={`"${name}" will start accepting connections again.`}
      confirmLabel="Resume"
      onConfirm={() => console.log("resume", name)}
    >
      <button
        disabled={false}
        className="flex items-center gap-1 border h-6 px-1.5 text-xs transition border-green-500/30 text-green-400 hover:bg-green-500/10"
      >
        <Play className="h-3 w-3" />
        resume
      </button>
    </ConfirmActionDialog>
  );
}
