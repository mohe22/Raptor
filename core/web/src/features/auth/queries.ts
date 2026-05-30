import { useMutation, useQuery } from "@tanstack/react-query";
import { getMe, loginUser } from "./api";

export function useMe() {
  return useQuery<{ userId: string } | null, Error>({
    queryKey: ["me"],
    queryFn: getMe,
    // retry: false,
    // staleTime: 1000 * 60 * 5,
  });
}
export function useLogin() {
  return useMutation({
    mutationFn: async ({
      username,
      password,
    }: {
      username: string;
      password: string;
    }) => await loginUser(username, password),
  });
}
