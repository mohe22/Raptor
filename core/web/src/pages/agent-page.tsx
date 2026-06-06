import { useState } from "react";
import { useParams } from "react-router";
import { useGetSessionById } from "../features/session/queries";
import { Header } from "../components/shared/header";
import { Tab } from "../components/session/tab";

export function SessionPage() {
  const { sessionId, serverId } = useParams();
  const [activeTab, setActiveTab] = useState("Interactive");

  const { data, isLoading, error } = useGetSessionById(serverId, sessionId);
  if (isLoading) return null;

  console.log(data, error);

  return (
    <>
      <Header title={"mohe"} subtitle={`10.0.01 | 127.0.0.1`} />
      <Tab activeTab={activeTab} setTab={setActiveTab} />
      {sessionId} - {serverId}
    </>
  );
}
