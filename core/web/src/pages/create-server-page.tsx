import { CreateNewServerForm } from "../components/forms/new-server-form";
import { Header } from "../components/shared/header";
import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "../components/ui/card";

export function CreateServerPage() {
  return (
    <>
      <Header
        title="Create New Server"
        subtitle="Configure a server endpoint for traffic routing and protocol handling."
      />

      <div className="p-2">
        <Card
          className="

            border-zinc-800
            bg-zinc-950/60
            shadow-2xl shadow-black/20
            backdrop-blur
          "
        >
          <CardHeader className="pb-2">
            <CardTitle className="text-xl">Server Configuration</CardTitle>

            <CardDescription>
              Enter connection details and network protocol settings.
            </CardDescription>
          </CardHeader>

          <CardContent>
            <CreateNewServerForm />
          </CardContent>
        </Card>
      </div>
    </>
  );
}
