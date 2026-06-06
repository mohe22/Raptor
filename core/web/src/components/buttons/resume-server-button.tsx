import { Play } from "lucide-react";
import { ConfirmActionDialog } from "../shared/confirm-action";
import { useResumeServer } from "../../features/servers/queries";

export function ResumeServerButton({ name }: { name: string }) {
  const { mutate, isPending, error } = useResumeServer();
  return (
    <ConfirmActionDialog
      title="Resume server?"
      description={`"${name}" will start accepting connections again.`}
      confirmLabel="Resume"
      onConfirm={() => {
        mutate(name);
        console.log("mm");
      }}
      isPending={isPending}
      error={error?.message ?? null}
    >
      <button
        disabled={isPending}
        className="flex items-center gap-1 border h-6 px-1.5 text-xs transition border-green-500/30 text-green-400 hover:bg-green-500/10"
      >
        <Play className="h-3 w-3" />
        resume
      </button>
    </ConfirmActionDialog>
  );
}
