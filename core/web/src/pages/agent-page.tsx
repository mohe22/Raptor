import { useSearchParams, useParams } from "react-router";
import { sessionOSConfig } from "../lib/data";
import { useGetSessionById } from "../features/session/queries";
import { Header } from "../components/shared/header";
import { Tab } from "../components/session/tab";
import { StatusBar } from "../components/session/status-bar";
import { Interactive } from "../components/session/Interactive";

const VALID_TABS = ["Interactive", "Info", "Plugins", "Network", "AI"] as const;
type TabLabel = (typeof VALID_TABS)[number];

export function SessionPage() {
  const { sessionId, serverId } = useParams() as {
    sessionId: string;
    serverId: string;
  };
  const [searchParams] = useSearchParams();

  const rawTab = searchParams.get("tab") ?? "Interactive";
  const activeTab: TabLabel = VALID_TABS.includes(rawTab as TabLabel)
    ? (rawTab as TabLabel)
    : "Interactive";

  const { data, isLoading, error } = useGetSessionById(serverId, sessionId);
  if (isLoading || error || !data) return null;

  const osConfig = sessionOSConfig[data.os];

  return (
    <>
      <Header
        title={data.hostname}
        subtitle={`${data.connectedTo} | ${data.remoteAddress}`}
      />
      <Tab activeTab={activeTab} />
      <StatusBar data={data} config={osConfig} />
      {activeTab === "Interactive" && (
        <Interactive sessionId={sessionId} serverId={serverId} />
      )}
    </>
  );
}
