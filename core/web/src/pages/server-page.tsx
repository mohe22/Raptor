import { useParams } from "react-router";
import { useGetServerById } from "../features/servers/queries";

export function ServerPage() {
  const { serverId } = useParams<{ serverId: string }>();
  const { data, isLoading, error } = useGetServerById(serverId);
  console.log(data);
  return <></>;
}
